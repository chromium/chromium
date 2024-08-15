// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/test/frame_segmenter_for_test.h"

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/media_tracks.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"
#include "media/parsers/h264_parser.h"

namespace chromecast {
namespace media {

namespace {

struct AudioFrameHeader {
  size_t offset;
  size_t frame_size;
  int sampling_frequency;
};

int mp3_bitrate[] = {
  0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
int mp3_sample_rate[] = { 44100, 48000, 32000, 0 };

AudioFrameHeader FindNextMp3Header(const uint8_t* data, size_t data_size) {
  bool found = false;
  AudioFrameHeader header;
  header.frame_size = 0;
  if (data_size < 4)
    return header;

  for (size_t k = 0; k < data_size - 4 && !found; k++) {
    // Mp3 Header:
    // syncword: 11111111111
    // Mpeg1: 11
    // Layer3: 01
    if (!(data[k + 0] == 0xff && (data[k + 1] & 0xfe) == 0xfa))
      continue;

    int bitrate_index = (data[k + 2] >> 4);
    if (bitrate_index == 0 || bitrate_index == 15) {
      // Free size or bad bitrate => not supported.
      continue;
    }

    int sample_rate_index = (data[k + 2] >> 2) & 0x3;
    if (sample_rate_index == 3)
      continue;

    size_t frame_size =
        ((1152 / 8) *  mp3_bitrate[bitrate_index] * 1000) /
        mp3_sample_rate[sample_rate_index];
    if (data[k + 2] & 0x2)
      frame_size++;

    // Make sure the frame is complete.
    if (k + frame_size > data_size)
      break;

    if (k + frame_size < data_size - 3 &&
        !(data[k + frame_size + 0] == 0xff &&
          (data[k + frame_size + 1] & 0xfe) == 0xfa)) {
      continue;
    }

    found = true;
    header.offset = k;
    header.frame_size = frame_size;
    header.sampling_frequency = mp3_sample_rate[sample_rate_index];
  }
  return header;
}

}  // namespace

BufferList Mp3SegmenterForTest(const uint8_t* data_ptr, size_t data_size) {
  // TODO(crbug.com/40284755): These functions should be based on span.
  auto data = UNSAFE_TODO(base::span(data_ptr, data_size));
  BufferList audio_frames;
  base::TimeDelta timestamp;

  while (true) {
    AudioFrameHeader header = FindNextMp3Header(data.data(), data.size());
    if (header.frame_size == 0) {
      break;
    }

    scoped_refptr<::media::DecoderBuffer> buffer(
        ::media::DecoderBuffer::CopyFrom(
            data.subspan(header.offset, header.frame_size)));
    data = data.subspan(header.offset + header.frame_size);
    buffer->set_timestamp(timestamp);
    audio_frames.push_back(
        scoped_refptr<DecoderBufferBase>(new DecoderBufferAdapter(buffer)));

    // 1152 samples in an MP3 frame.
    timestamp += base::Microseconds((UINT64_C(1152) * 1000 * 1000) /
                                    header.sampling_frequency);
  }
  return audio_frames;
}

struct H264AccessUnit {
  H264AccessUnit();

  size_t offset;
  size_t size;
  int has_vcl;
  int poc;
};

H264AccessUnit::H264AccessUnit()
  : offset(0),
    size(0),
    has_vcl(false),
    poc(0) {
}

BufferList H264SegmenterForTest(const uint8_t* data_ptr, size_t data_size) {
  // TODO(crbug.com/40284755): These functions should be based on span.
  auto data = UNSAFE_TODO(base::span(data_ptr, data_size));
  BufferList video_frames;
  std::list<H264AccessUnit> access_unit_list;
  H264AccessUnit access_unit;

  int prev_pic_order_cnt_lsb = 0;
  int pic_order_cnt_msb = 0;

  std::unique_ptr<::media::H264Parser> h264_parser(new ::media::H264Parser());
  h264_parser->SetStream(data_ptr, data_size);

  while (true) {
    bool is_eos = false;
    ::media::H264NALU nalu;
    switch (h264_parser->AdvanceToNextNALU(&nalu)) {
      case ::media::H264Parser::kOk:
        break;
      case ::media::H264Parser::kInvalidStream:
      case ::media::H264Parser::kUnsupportedStream:
        return video_frames;
      case ::media::H264Parser::kEOStream:
        is_eos = true;
        break;
    }
    if (is_eos)
      break;

    // To get the NALU syncword offset, substract 3 or 4
    // which corresponds to the possible syncword lengths.
    size_t nalu_offset = nalu.data - data_ptr;
    nalu_offset -= 3;
    if (nalu_offset > 0 && data[nalu_offset-1] == 0)
      nalu_offset--;

    switch (nalu.nal_unit_type) {
      case ::media::H264NALU::kAUD: {
        break;
      }
      case ::media::H264NALU::kSPS: {
        int sps_id;
        if (h264_parser->ParseSPS(&sps_id) != ::media::H264Parser::kOk)
          return video_frames;
        if (access_unit.has_vcl) {
          access_unit.size = nalu_offset - access_unit.offset;
          access_unit_list.push_back(access_unit);
          access_unit = H264AccessUnit();
          access_unit.offset = nalu_offset;
        }
        break;
      }
      case ::media::H264NALU::kPPS: {
        int pps_id;
        if (h264_parser->ParsePPS(&pps_id) != ::media::H264Parser::kOk)
          return video_frames;
        if (access_unit.has_vcl) {
          access_unit.size = nalu_offset - access_unit.offset;
          access_unit_list.push_back(access_unit);
          access_unit = H264AccessUnit();
          access_unit.offset = nalu_offset;
        }
        break;
      }
      case ::media::H264NALU::kIDRSlice:
      case ::media::H264NALU::kNonIDRSlice: {
        ::media::H264SliceHeader shdr;
        if (h264_parser->ParseSliceHeader(nalu, &shdr) !=
            ::media::H264Parser::kOk) {
          return video_frames;
        }
        const ::media::H264PPS* pps =
            h264_parser->GetPPS(shdr.pic_parameter_set_id);
        if (!pps)
          return video_frames;
        const ::media::H264SPS* sps =
            h264_parser->GetSPS(pps->seq_parameter_set_id);

        // Very simplified way to segment H264.
        // This assumes only 1 VCL NALU per access unit.
        if (access_unit.has_vcl) {
          access_unit.size = nalu_offset - access_unit.offset;
          access_unit_list.push_back(access_unit);
          access_unit = H264AccessUnit();
          access_unit.offset = nalu_offset;
        }

        access_unit.has_vcl = true;

        // Support only explicit POC so far.
        if (sps->pic_order_cnt_type != 0) {
          LOG(WARNING) << "Unsupported pic_order_cnt_type";
          return video_frames;
        }
        int diff_pic_order_cnt_lsb =
            shdr.pic_order_cnt_lsb - prev_pic_order_cnt_lsb;
        int max_pic_order_cnt_lsb =
            1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
        if (diff_pic_order_cnt_lsb < 0 &&
            diff_pic_order_cnt_lsb <= -max_pic_order_cnt_lsb / 2) {
          pic_order_cnt_msb += max_pic_order_cnt_lsb;
        } else if (diff_pic_order_cnt_lsb > 0 &&
                   diff_pic_order_cnt_lsb > max_pic_order_cnt_lsb / 2) {
          pic_order_cnt_msb -= max_pic_order_cnt_lsb;
        }
        access_unit.poc = pic_order_cnt_msb + shdr.pic_order_cnt_lsb;
        prev_pic_order_cnt_lsb = shdr.pic_order_cnt_lsb;
        break;
      }
      default: {
      }
    }
  }

  // Emit the last access unit.
  if (access_unit.has_vcl) {
    access_unit.size = data_size - access_unit.offset;
    access_unit_list.push_back(access_unit);
  }

  // Create the list of buffers.
  // Totally arbitrary decision: assume a delta POC of 1 is 20ms (50Hz field
  // rate).
  base::TimeDelta poc_duration = base::Milliseconds(20);
  for (std::list<H264AccessUnit>::iterator it = access_unit_list.begin();
       it != access_unit_list.end(); ++it) {
    scoped_refptr<::media::DecoderBuffer> buffer(
        ::media::DecoderBuffer::CopyFrom(data.subspan(it->offset, it->size)));
    buffer->set_timestamp(it->poc * poc_duration);
    video_frames.push_back(
        scoped_refptr<DecoderBufferBase>(new DecoderBufferAdapter(buffer)));
  }

  return video_frames;
}

void OnEncryptedMediaInitData(::media::EmeInitDataType init_data_type,
                              const std::vector<uint8_t>& init_data) {
  LOG(FATAL) << "Unexpected test failure: file is encrypted.";
}

void OnMediaTracksUpdated(std::unique_ptr<::media::MediaTracks> tracks) {}

void OnNewBuffer(BufferList* buffer_list,
                 const base::RepeatingClosure& finished_cb,
                 ::media::DemuxerStream::Status status,
                 ::media::DemuxerStream::DecoderBufferVector buffers) {
  CHECK_EQ(status, ::media::DemuxerStream::kOk);
  EXPECT_EQ(buffers.size(), 1u) << "OnNewBuffer only reads a single buffer.";
  scoped_refptr<::media::DecoderBuffer> buffer = std::move(buffers[0]);
  CHECK(buffer.get());
  CHECK(buffer_list);
  buffer_list->push_back(new DecoderBufferAdapter(buffer));
  finished_cb.Run();
}

class FakeDemuxerHost : public ::media::DemuxerHost {
 public:
  // DemuxerHost implementation.
  void OnBufferedTimeRangesChanged(
      const ::media::Ranges<base::TimeDelta>& ranges) override {}
  void SetDuration(base::TimeDelta duration) override {}
  void OnDemuxerError(::media::PipelineStatus error) override {
    LOG(FATAL) << "OnDemuxerError: " << error;
  }
};

DemuxResult::DemuxResult() {
}

DemuxResult::DemuxResult(const DemuxResult& other) = default;

DemuxResult::~DemuxResult() {
}

DemuxResult FFmpegDemuxForTest(const base::FilePath& filepath,
                               bool audio) {
  FakeDemuxerHost fake_demuxer_host;
  ::media::FileDataSource data_source;
  CHECK(data_source.Initialize(filepath));

  ::media::NullMediaLog media_log;
  ::media::FFmpegDemuxer demuxer(
      base::SingleThreadTaskRunner::GetCurrentDefault(), &data_source,
      base::BindRepeating(&OnEncryptedMediaInitData),
      base::BindRepeating(&OnMediaTracksUpdated), &media_log, true);
  ::media::WaitableMessageLoopEvent init_event;
  demuxer.Initialize(&fake_demuxer_host, init_event.GetPipelineStatusCB());
  init_event.RunAndWaitForStatus(::media::PIPELINE_OK);

  auto stream_type =
      audio ? ::media::DemuxerStream::AUDIO : ::media::DemuxerStream::VIDEO;
  ::media::DemuxerStream* stream = demuxer.GetFirstStream(stream_type);
  CHECK(stream);
  stream->EnableBitstreamConverter();

  DemuxResult demux_result;
  if (audio) {
    demux_result.audio_config = stream->audio_decoder_config();
  } else {
    demux_result.video_config = stream->video_decoder_config();
  }

  bool end_of_stream = false;
  while (!end_of_stream) {
    base::RunLoop run_loop;
    stream->Read(
        1, base::BindOnce(&OnNewBuffer, base::Unretained(&demux_result.frames),
                          run_loop.QuitClosure()));
    run_loop.Run();
    CHECK(!demux_result.frames.empty());
    end_of_stream = demux_result.frames.back()->end_of_stream();
  }

  demuxer.Stop();
  return demux_result;
}

}  // namespace media
}  // namespace chromecast
