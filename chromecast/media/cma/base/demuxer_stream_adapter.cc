// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/demuxer_stream_adapter.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/simple_media_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

DemuxerStreamAdapter::DemuxerStreamAdapter(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<BalancedMediaTaskRunnerFactory>&
        media_task_runner_factory,
    ::media::DemuxerStream* demuxer_stream)
    : task_runner_(task_runner),
      media_task_runner_factory_(media_task_runner_factory),
      media_task_runner_(new SimpleMediaTaskRunner(task_runner)),
      demuxer_stream_(demuxer_stream),
      is_pending_read_(false),
      is_pending_demuxer_read_(false),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  ResetMediaTaskRunner();
  thread_checker_.DetachFromThread();
}

DemuxerStreamAdapter::~DemuxerStreamAdapter() {
  // Needed since we use weak pointers:
  // weak pointers must be invalidated on the same thread.
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DemuxerStreamAdapter::Read(const ReadCB& read_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(flush_cb_.is_null());

  // Support only one read at a time.
  DCHECK(!is_pending_read_);
  is_pending_read_ = true;
  ReadInternal(read_cb);
}

void DemuxerStreamAdapter::ReadInternal(const ReadCB& read_cb) {
  bool may_run_in_future = media_task_runner_->PostMediaTask(
      FROM_HERE,
      base::Bind(&DemuxerStreamAdapter::RequestBuffer, weak_this_, read_cb),
      max_pts_);
  DCHECK(may_run_in_future);
}

void DemuxerStreamAdapter::Flush(const base::Closure& flush_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(INFO) << __FUNCTION__;

  // Flush cancels any pending read.
  is_pending_read_ = false;

  // Reset the decoder configurations.
  audio_config_ = ::media::AudioDecoderConfig();
  video_config_ = ::media::VideoDecoderConfig();

  // Create a new media task runner for the upcoming media timeline.
  ResetMediaTaskRunner();

  DCHECK(flush_cb_.is_null());
  if (is_pending_demuxer_read_) {
    // If there is a pending demuxer read, the implicit contract
    // is that the pending read must be completed before invoking the
    // flush callback.
    flush_cb_ = flush_cb;
    return;
  }

  // At this point, there is no more pending demuxer read,
  // so all the previous tasks associated with the current timeline
  // can be cancelled.
  weak_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_factory_.GetWeakPtr();

  LOG(INFO) << "Flush done";
  flush_cb.Run();
}

void DemuxerStreamAdapter::ResetMediaTaskRunner() {
  DCHECK(thread_checker_.CalledOnValidThread());

  max_pts_ = ::media::kNoTimestamp;
  if (media_task_runner_factory_.get()) {
    media_task_runner_ =
        media_task_runner_factory_->CreateMediaTaskRunner(task_runner_);
  }
}

void DemuxerStreamAdapter::RequestBuffer(const ReadCB& read_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_pending_demuxer_read_ = true;
  demuxer_stream_->Read(::media::BindToCurrentLoop(
      base::Bind(&DemuxerStreamAdapter::OnNewBuffer, weak_this_, read_cb)));
}

void DemuxerStreamAdapter::OnNewBuffer(
    const ReadCB& read_cb,
    ::media::DemuxerStream::Status status,
    scoped_refptr<::media::DecoderBuffer> input) {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_pending_demuxer_read_ = false;

  // Just discard the buffer in the flush stage.
  if (!flush_cb_.is_null()) {
    LOG(INFO) << "Flush done";
    std::move(flush_cb_).Run();
    return;
  }

  if (status == ::media::DemuxerStream::kAborted) {
    DCHECK(!input);
    return;
  }

  if (status == ::media::DemuxerStream::kConfigChanged) {
    DCHECK(!input);
    if (demuxer_stream_->type() == ::media::DemuxerStream::VIDEO)
      video_config_ = demuxer_stream_->video_decoder_config();
    if (demuxer_stream_->type() == ::media::DemuxerStream::AUDIO)
      audio_config_ = demuxer_stream_->audio_decoder_config();

    // Got a new config, but we still need to get a frame.
    ReadInternal(read_cb);
    return;
  }

  DCHECK_EQ(status, ::media::DemuxerStream::kOk);

  if (input->end_of_stream()) {
    // This stream has ended, its media time will stop increasing, but there
    // might be other streams that are still playing. Remove the task runner of
    // this stream to ensure other streams are not blocked waiting for this one.
    ResetMediaTaskRunner();
  }

  // Updates the timestamp used for task scheduling.
  if (!input->end_of_stream() && input->timestamp() != ::media::kNoTimestamp &&
      (max_pts_ == ::media::kNoTimestamp || input->timestamp() > max_pts_)) {
    max_pts_ = input->timestamp();
  }

  // Provides the buffer as well as possibly valid audio and video configs.
  is_pending_read_ = false;
  scoped_refptr<DecoderBufferBase> buffer(
      new DecoderBufferAdapter(std::move(input)));
  read_cb.Run(buffer, audio_config_, video_config_);

  // Back to the default audio/video config:
  // an invalid audio/video config means there is no config update.
  if (audio_config_.IsValidConfig())
    audio_config_ = ::media::AudioDecoderConfig();
  if (video_config_.IsValidConfig())
    video_config_ = ::media::VideoDecoderConfig();
}

}  // namespace media
}  // namespace chromecast
