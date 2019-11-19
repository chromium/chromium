// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/cast_streaming_native_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/extensions/api/cast_streaming_receiver_session.h"
#include "chrome/common/extensions/api/cast_streaming_rtp_stream.h"
#include "chrome/common/extensions/api/cast_streaming_udp_transport.h"
#include "chrome/renderer/media/cast_receiver_session.h"
#include "chrome/renderer/media/cast_rtp_stream.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_dom_media_stream_track.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

using content::V8ValueConverter;
using media::cast::FrameSenderConfig;

// Extension types.
using extensions::api::cast_streaming_receiver_session::RtpReceiverParams;
using extensions::api::cast_streaming_rtp_stream::RtpParams;
using extensions::api::cast_streaming_rtp_stream::RtpPayloadParams;
using extensions::api::cast_streaming_udp_transport::IPEndPoint;

namespace extensions {

namespace {

constexpr char kInvalidAesIvMask[] = "Invalid value for AES IV mask";
constexpr char kInvalidAesKey[] = "Invalid value for AES key";
constexpr char kInvalidDestination[] = "Invalid destination";
constexpr char kInvalidRtpParams[] = "Invalid value for RTP params";
constexpr char kInvalidStreamArgs[] = "Invalid stream arguments";
constexpr char kRtpStreamNotFound[] = "The RTP stream cannot be found";
constexpr char kUdpTransportNotFound[] = "The UDP transport cannot be found";
constexpr char kUnableToConvertArgs[] = "Unable to convert arguments";
constexpr char kUnableToConvertParams[] = "Unable to convert params";
constexpr char kCodecNameOpus[] = "OPUS";
constexpr char kCodecNameVp8[] = "VP8";
constexpr char kCodecNameH264[] = "H264";
constexpr char kCodecNameRemoteAudio[] = "REMOTE_AUDIO";
constexpr char kCodecNameRemoteVideo[] = "REMOTE_VIDEO";

// To convert from kilobits per second to bits per second.
constexpr int kBitsPerKilobit = 1000;

bool HexDecode(const std::string& input, std::string* output) {
  DCHECK(output->empty());
  if (!base::HexStringToString(input, output)) {
    output->clear();
    return false;
  }
  return true;
}

int NumberOfEncodeThreads() {
  // Do not saturate CPU utilization just for encoding. On a lower-end system
  // with only 1 or 2 cores, use only one thread for encoding. On systems with
  // more cores, allow half of the cores to be used for encoding.
  return std::min(8, (base::SysInfo::NumberOfProcessors() + 1) / 2);
}

bool ToFrameSenderConfigOrThrow(v8::Isolate* isolate,
                                const RtpPayloadParams& ext_params,
                                FrameSenderConfig* config) {
  config->sender_ssrc = ext_params.ssrc;
  config->receiver_ssrc = ext_params.feedback_ssrc;
  if (config->sender_ssrc == config->receiver_ssrc) {
    DVLOG(1) << "sender_ssrc " << config->sender_ssrc
             << " cannot be equal to receiver_ssrc";
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return false;
  }
  config->min_playout_delay = base::TimeDelta::FromMilliseconds(
      ext_params.min_latency ? *ext_params.min_latency
                             : ext_params.max_latency);
  config->max_playout_delay =
      base::TimeDelta::FromMilliseconds(ext_params.max_latency);
  config->animated_playout_delay = base::TimeDelta::FromMilliseconds(
      ext_params.animated_latency ? *ext_params.animated_latency
                                  : ext_params.max_latency);
  if (config->min_playout_delay <= base::TimeDelta()) {
    DVLOG(1) << "min_playout_delay " << config->min_playout_delay
             << " must be greater than zero";
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return false;
  }
  if (config->min_playout_delay > config->max_playout_delay) {
    DVLOG(1) << "min_playout_delay " << config->min_playout_delay
             << " must be less than or equal to max_palyout_delay";
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return false;
  }
  if (config->animated_playout_delay < config->min_playout_delay ||
      config->animated_playout_delay > config->max_playout_delay) {
    DVLOG(1) << "animated_playout_delay " << config->animated_playout_delay
             << " must be between (inclusive) the min and max playout delay";
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return false;
  }
  if (ext_params.codec_name == kCodecNameOpus) {
    config->rtp_payload_type = media::cast::RtpPayloadType::AUDIO_OPUS;
    config->use_external_encoder = false;
    config->rtp_timebase = ext_params.clock_rate
                               ? *ext_params.clock_rate
                               : media::cast::kDefaultAudioSamplingRate;
    // Sampling rate must be one of the Opus-supported values.
    switch (config->rtp_timebase) {
      case 48000:
      case 24000:
      case 16000:
      case 12000:
      case 8000:
        break;
      default:
        DVLOG(1) << "rtp_timebase " << config->rtp_timebase << " is invalid";
        isolate->ThrowException(v8::Exception::Error(
            v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                    v8::NewStringType::kNormal)
                .ToLocalChecked()));
        return false;
    }
    config->channels = ext_params.channels ? *ext_params.channels : 2;
    if (config->channels != 1 && config->channels != 2) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                  v8::NewStringType::kNormal)
              .ToLocalChecked()));
      DVLOG(1) << "channels " << config->channels << " is invalid";
      return false;
    }
    config->min_bitrate = config->start_bitrate = config->max_bitrate =
        ext_params.max_bitrate ? (*ext_params.max_bitrate) * kBitsPerKilobit
                               : media::cast::kDefaultAudioEncoderBitrate;
    config->max_frame_rate = 100;  // 10ms audio frames.
    config->codec = media::cast::CODEC_AUDIO_OPUS;
  } else if (ext_params.codec_name == kCodecNameVp8 ||
             ext_params.codec_name == kCodecNameH264) {
    config->rtp_timebase = media::cast::kVideoFrequency;
    config->channels = ext_params.channels ? *ext_params.channels : 1;
    if (config->channels != 1) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                  v8::NewStringType::kNormal)
              .ToLocalChecked()));
      DVLOG(1) << "channels " << config->channels << " is invalid";
      return false;
    }
    config->min_bitrate = ext_params.min_bitrate
                              ? (*ext_params.min_bitrate) * kBitsPerKilobit
                              : media::cast::kDefaultMinVideoBitrate;
    config->max_bitrate = ext_params.max_bitrate
                              ? (*ext_params.max_bitrate) * kBitsPerKilobit
                              : media::cast::kDefaultMaxVideoBitrate;
    if (config->min_bitrate > config->max_bitrate) {
      DVLOG(1) << "min_bitrate " << config->min_bitrate << " is larger than "
               << "max_bitrate " << config->max_bitrate;
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                  v8::NewStringType::kNormal)
              .ToLocalChecked()));
      return false;
    }
    config->start_bitrate = config->min_bitrate;
    config->max_frame_rate = std::max(
        1.0, ext_params.max_frame_rate ? *ext_params.max_frame_rate : 0.0);
    if (config->max_frame_rate > media::limits::kMaxFramesPerSecond) {
      DVLOG(1) << "max_frame_rate " << config->max_frame_rate << " is invalid";
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                  v8::NewStringType::kNormal)
              .ToLocalChecked()));
      return false;
    }
    if (ext_params.codec_name == kCodecNameVp8) {
      config->rtp_payload_type = media::cast::RtpPayloadType::VIDEO_VP8;
      config->codec = media::cast::CODEC_VIDEO_VP8;
      config->use_external_encoder =
          CastRtpStream::IsHardwareVP8EncodingSupported();
    } else {
      config->rtp_payload_type = media::cast::RtpPayloadType::VIDEO_H264;
      config->codec = media::cast::CODEC_VIDEO_H264;
      config->use_external_encoder =
          CastRtpStream::IsHardwareH264EncodingSupported();
    }
    if (!config->use_external_encoder)
      config->video_codec_params.number_of_encode_threads =
          NumberOfEncodeThreads();
  } else if (ext_params.codec_name == kCodecNameRemoteAudio) {
    config->rtp_payload_type = media::cast::RtpPayloadType::REMOTE_AUDIO;
    config->codec = media::cast::CODEC_AUDIO_REMOTE;
  } else if (ext_params.codec_name == kCodecNameRemoteVideo) {
    config->rtp_payload_type = media::cast::RtpPayloadType::REMOTE_VIDEO;
    config->codec = media::cast::CODEC_VIDEO_REMOTE;
  } else {
    DVLOG(1) << "codec_name " << ext_params.codec_name << " is invalid";
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kInvalidRtpParams,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return false;
  }
  if (ext_params.aes_key && !HexDecode(*ext_params.aes_key, &config->aes_key)) {
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kInvalidAesKey,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return false;
  }
  if (ext_params.aes_iv_mask &&
      !HexDecode(*ext_params.aes_iv_mask, &config->aes_iv_mask)) {
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kInvalidAesIvMask,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return false;
  }
  return true;
}

void FromFrameSenderConfig(const FrameSenderConfig& config,
                           RtpPayloadParams* ext_params) {
  ext_params->payload_type = static_cast<int>(config.rtp_payload_type);
  ext_params->max_latency = config.max_playout_delay.InMilliseconds();
  ext_params->min_latency.reset(
      new int(config.min_playout_delay.InMilliseconds()));
  ext_params->animated_latency.reset(
      new int(config.animated_playout_delay.InMilliseconds()));
  switch (config.codec) {
    case media::cast::CODEC_AUDIO_OPUS:
      ext_params->codec_name = kCodecNameOpus;
      break;
    case media::cast::CODEC_VIDEO_VP8:
      ext_params->codec_name = kCodecNameVp8;
      break;
    case media::cast::CODEC_VIDEO_H264:
      ext_params->codec_name = kCodecNameH264;
      break;
    case media::cast::CODEC_AUDIO_REMOTE:
      ext_params->codec_name = kCodecNameRemoteAudio;
      break;
    case media::cast::CODEC_VIDEO_REMOTE:
      ext_params->codec_name = kCodecNameRemoteVideo;
      break;
    default:
      NOTREACHED();
  }
  ext_params->ssrc = config.sender_ssrc;
  ext_params->feedback_ssrc = config.receiver_ssrc;
  if (config.rtp_timebase)
    ext_params->clock_rate.reset(new int(config.rtp_timebase));
  if (config.min_bitrate)
    ext_params->min_bitrate.reset(
        new int(config.min_bitrate / kBitsPerKilobit));
  if (config.max_bitrate)
    ext_params->max_bitrate.reset(
        new int(config.max_bitrate / kBitsPerKilobit));
  if (config.channels)
    ext_params->channels.reset(new int(config.channels));
  if (config.max_frame_rate > 0.0)
    ext_params->max_frame_rate.reset(new double(config.max_frame_rate));
}

}  // namespace

// |last_transport_id_| is the identifier for the next created RTP stream. To
// create globally unique IDs used for referring to RTP stream objects in
// browser process, we set its higher 16 bits as HASH(extension_id)&0x7fff, and
// lower 16 bits as the sequence number of the RTP stream created in the same
// extension. Collision will happen when the first RTP stream keeps alive after
// creating another 64k-1 RTP streams in the same extension, which is very
// unlikely to happen in normal use cases.
CastStreamingNativeHandler::CastStreamingNativeHandler(
    ScriptContext* context,
    NativeExtensionBindingsSystem* bindings_system)
    : ObjectBackedNativeHandler(context),
      last_transport_id_(
          context->extension()
              ? (((base::Hash(context->extension()->id()) & 0x7fff) << 16) + 1)
              : 1),
      bindings_system_(bindings_system) {}

CastStreamingNativeHandler::~CastStreamingNativeHandler() {
  // Note: A superclass's destructor will call Invalidate(), but Invalidate()
  // may also be called at any time before destruction.
}

void CastStreamingNativeHandler::AddRoutes() {
  RouteHandlerFunction(
      "CreateSession", "cast.streaming.session",
      base::BindRepeating(&CastStreamingNativeHandler::CreateCastSession,
                          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "DestroyCastRtpStream", "cast.streaming.rtpStream",
      base::BindRepeating(&CastStreamingNativeHandler::DestroyCastRtpStream,
                          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "GetSupportedParamsCastRtpStream", "cast.streaming.rtpStream",
      base::BindRepeating(
          &CastStreamingNativeHandler::GetSupportedParamsCastRtpStream,
          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "StartCastRtpStream", "cast.streaming.rtpStream",
      base::BindRepeating(&CastStreamingNativeHandler::StartCastRtpStream,
                          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "StopCastRtpStream", "cast.streaming.rtpStream",
      base::BindRepeating(&CastStreamingNativeHandler::StopCastRtpStream,
                          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "DestroyCastUdpTransport", "cast.streaming.udpTransport",
      base::BindRepeating(&CastStreamingNativeHandler::DestroyCastUdpTransport,
                          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "SetDestinationCastUdpTransport", "cast.streaming.udpTransport",
      base::BindRepeating(
          &CastStreamingNativeHandler::SetDestinationCastUdpTransport,
          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "SetOptionsCastUdpTransport", "cast.streaming.udpTransport",
      base::BindRepeating(
          &CastStreamingNativeHandler::SetOptionsCastUdpTransport,
          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "ToggleLogging", "cast.streaming.rtpStream",
      base::BindRepeating(&CastStreamingNativeHandler::ToggleLogging,
                          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "GetRawEvents", "cast.streaming.rtpStream",
      base::BindRepeating(&CastStreamingNativeHandler::GetRawEvents,
                          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "GetStats", "cast.streaming.rtpStream",
      base::BindRepeating(&CastStreamingNativeHandler::GetStats,
                          weak_factory_.GetWeakPtr()));
}

void CastStreamingNativeHandler::Invalidate() {
  // Cancel all function call routing and callbacks.
  weak_factory_.InvalidateWeakPtrs();

  // Clear all references to V8 and Cast objects, which will trigger
  // auto-destructions (effectively stopping all sessions).
  get_stats_callbacks_.clear();
  get_raw_events_callbacks_.clear();
  create_callback_.Reset();
  udp_transport_map_.clear();
  rtp_stream_map_.clear();

  ObjectBackedNativeHandler::Invalidate();
}

void CastStreamingNativeHandler::CreateCastSession(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[2]->IsFunction());

  v8::Isolate* isolate = context()->v8_context()->GetIsolate();

  auto session = base::MakeRefCounted<CastSession>(
      context()->web_frame()->GetTaskRunner(blink::TaskType::kInternalMedia));
  std::unique_ptr<CastRtpStream> stream1, stream2;
  if ((args[0]->IsNull() || args[0]->IsUndefined()) &&
      (args[1]->IsNull() || args[1]->IsUndefined())) {
    DVLOG(3) << "CreateCastSession for remoting.";
    // Creates audio/video RTP streams for media remoting.
    stream1.reset(new CastRtpStream(true, session));
    stream2.reset(new CastRtpStream(false, session));
  } else {
    // Creates RTP streams that consume from an audio and/or a video
    // MediaStreamTrack.
    if (!args[0]->IsNull() && !args[0]->IsUndefined()) {
      CHECK(args[0]->IsObject());
      blink::WebDOMMediaStreamTrack track =
          blink::WebDOMMediaStreamTrack::FromV8Value(args[0]);
      if (track.IsNull()) {
        isolate->ThrowException(v8::Exception::Error(
            v8::String::NewFromUtf8(isolate, kInvalidStreamArgs,
                                    v8::NewStringType::kNormal)
                .ToLocalChecked()));
        return;
      }
      stream1.reset(new CastRtpStream(track.Component(), session));
    }
    if (!args[1]->IsNull() && !args[1]->IsUndefined()) {
      CHECK(args[1]->IsObject());
      blink::WebDOMMediaStreamTrack track =
          blink::WebDOMMediaStreamTrack::FromV8Value(args[1]);
      if (track.IsNull()) {
        isolate->ThrowException(v8::Exception::Error(
            v8::String::NewFromUtf8(isolate, kInvalidStreamArgs,
                                    v8::NewStringType::kNormal)
                .ToLocalChecked()));
        return;
      }
      stream2.reset(new CastRtpStream(track.Component(), session));
    }
  }
  std::unique_ptr<CastUdpTransport> udp_transport(
      new CastUdpTransport(session));

  create_callback_.Reset(isolate, args[2].As<v8::Function>());

  context()
      ->web_frame()
      ->GetTaskRunner(blink::TaskType::kInternalMedia)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&CastStreamingNativeHandler::CallCreateCallback,
                         weak_factory_.GetWeakPtr(), base::Passed(&stream1),
                         base::Passed(&stream2), base::Passed(&udp_transport)));
}

void CastStreamingNativeHandler::CallCreateCallback(
    std::unique_ptr<CastRtpStream> stream1,
    std::unique_ptr<CastRtpStream> stream2,
    std::unique_ptr<CastUdpTransport> udp_transport) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  v8::Local<v8::Value> callback_args[3];
  callback_args[0] = v8::Null(isolate);
  callback_args[1] = v8::Null(isolate);

  if (stream1) {
    const int stream1_id = last_transport_id_++;
    callback_args[0] = v8::Integer::New(isolate, stream1_id);
    rtp_stream_map_[stream1_id] = std::move(stream1);
  }
  if (stream2) {
    const int stream2_id = last_transport_id_++;
    callback_args[1] = v8::Integer::New(isolate, stream2_id);
    rtp_stream_map_[stream2_id] = std::move(stream2);
  }
  const int udp_id = last_transport_id_++;
  udp_transport_map_[udp_id] = std::move(udp_transport);
  callback_args[2] = v8::Integer::New(isolate, udp_id);
  context()->SafeCallFunction(
      v8::Local<v8::Function>::New(isolate, create_callback_), 3,
      callback_args);
  create_callback_.Reset();
}

void CastStreamingNativeHandler::CallStartCallback(int stream_id) const {
  base::ListValue event_args;
  event_args.AppendInteger(stream_id);
  bindings_system_->DispatchEventInContext("cast.streaming.rtpStream.onStarted",
                                           &event_args, nullptr, context());
}

void CastStreamingNativeHandler::CallStopCallback(int stream_id) const {
  base::ListValue event_args;
  event_args.AppendInteger(stream_id);
  bindings_system_->DispatchEventInContext("cast.streaming.rtpStream.onStopped",
                                           &event_args, nullptr, context());
}

void CastStreamingNativeHandler::CallErrorCallback(
    int stream_id,
    const std::string& message) const {
  base::ListValue event_args;
  event_args.AppendInteger(stream_id);
  event_args.AppendString(message);
  bindings_system_->DispatchEventInContext("cast.streaming.rtpStream.onError",
                                           &event_args, nullptr, context());
}

void CastStreamingNativeHandler::DestroyCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0].As<v8::Int32>()->Value();
  if (!GetRtpStreamOrThrow(transport_id))
    return;
  rtp_stream_map_.erase(transport_id);
}

void CastStreamingNativeHandler::GetSupportedParamsCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) const {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0].As<v8::Int32>()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  std::unique_ptr<V8ValueConverter> converter = V8ValueConverter::Create();
  std::vector<FrameSenderConfig> configs = transport->GetSupportedConfigs();
  v8::Local<v8::Array> result =
      v8::Array::New(args.GetIsolate(), static_cast<int>(configs.size()));
  for (size_t i = 0; i < configs.size(); ++i) {
    RtpParams params;
    FromFrameSenderConfig(configs[i], &params.payload);
    std::unique_ptr<base::DictionaryValue> params_value = params.ToValue();
    result
        ->CreateDataProperty(
            context()->v8_context(), static_cast<int>(i),
            converter->ToV8Value(params_value.get(), context()->v8_context()))
        .Check();
  }
  args.GetReturnValue().Set(result);
}

void CastStreamingNativeHandler::StartCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0].As<v8::Int32>()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  std::unique_ptr<base::Value> params_value =
      V8ValueConverter::Create()->FromV8Value(args[1], context()->v8_context());
  if (!params_value) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertParams,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return;
  }
  std::unique_ptr<RtpParams> params = RtpParams::FromValue(*params_value);
  if (!params) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kInvalidRtpParams,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return;
  }

  FrameSenderConfig config;
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  if (!ToFrameSenderConfigOrThrow(isolate, params->payload, &config))
    return;

  base::Closure start_callback =
      base::Bind(&CastStreamingNativeHandler::CallStartCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id);
  base::Closure stop_callback =
      base::Bind(&CastStreamingNativeHandler::CallStopCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id);
  CastRtpStream::ErrorCallback error_callback =
      base::Bind(&CastStreamingNativeHandler::CallErrorCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id);

  // |transport_id| is a globally unique identifier for the RTP stream.
  transport->Start(transport_id, config, start_callback, stop_callback,
                   error_callback);
}

void CastStreamingNativeHandler::StopCastRtpStream(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0].As<v8::Int32>()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;
  transport->Stop();
}

void CastStreamingNativeHandler::DestroyCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  const int transport_id = args[0].As<v8::Int32>()->Value();
  if (!GetUdpTransportOrThrow(transport_id))
    return;
  udp_transport_map_.erase(transport_id);
}

void CastStreamingNativeHandler::SetDestinationCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0].As<v8::Int32>()->Value();
  CastUdpTransport* transport = GetUdpTransportOrThrow(transport_id);
  if (!transport)
    return;

  net::IPEndPoint dest;
  if (!IPEndPointFromArg(args.GetIsolate(),
                         args[1],
                         &dest)) {
    return;
  }
  transport->SetDestination(
      dest,
      base::Bind(&CastStreamingNativeHandler::CallErrorCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id));
}

void CastStreamingNativeHandler::SetOptionsCastUdpTransport(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsObject());

  const int transport_id = args[0].As<v8::Int32>()->Value();
  CastUdpTransport* transport = GetUdpTransportOrThrow(transport_id);
  if (!transport)
    return;

  std::unique_ptr<base::DictionaryValue> options =
      base::DictionaryValue::From(V8ValueConverter::Create()->FromV8Value(
          args[1], context()->v8_context()));
  if (!options) {
    args.GetIsolate()->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(args.GetIsolate(), kUnableToConvertArgs,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return;
  }
  transport->SetOptions(std::move(options));
}

void CastStreamingNativeHandler::ToggleLogging(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());

  const int stream_id = args[0].As<v8::Int32>()->Value();
  CastRtpStream* stream = GetRtpStreamOrThrow(stream_id);
  if (!stream)
    return;

  const bool enable = args[1]->ToBoolean(args.GetIsolate())->Value();
  stream->ToggleLogging(enable);
}

void CastStreamingNativeHandler::GetRawEvents(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(3, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsNull() || args[1]->IsString());
  CHECK(args[2]->IsFunction());

  const int transport_id = args[0].As<v8::Int32>()->Value();
  v8::Isolate* isolate = args.GetIsolate();
  v8::Global<v8::Function> callback(isolate, args[2].As<v8::Function>());
  std::string extra_data;
  if (!args[1]->IsNull()) {
    extra_data = *v8::String::Utf8Value(isolate, args[1]);
  }

  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  get_raw_events_callbacks_.insert(
      std::make_pair(transport_id, std::move(callback)));

  transport->GetRawEvents(
      base::Bind(&CastStreamingNativeHandler::CallGetRawEventsCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id),
      extra_data);
}

void CastStreamingNativeHandler::GetStats(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());
  const int transport_id = args[0].As<v8::Int32>()->Value();
  CastRtpStream* transport = GetRtpStreamOrThrow(transport_id);
  if (!transport)
    return;

  v8::Global<v8::Function> callback(args.GetIsolate(),
                                    args[1].As<v8::Function>());
  get_stats_callbacks_.insert(
      std::make_pair(transport_id, std::move(callback)));

  transport->GetStats(
      base::Bind(&CastStreamingNativeHandler::CallGetStatsCallback,
                 weak_factory_.GetWeakPtr(),
                 transport_id));
}

void CastStreamingNativeHandler::CallGetRawEventsCallback(
    int transport_id,
    std::unique_ptr<base::Value> raw_events) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  auto it = get_raw_events_callbacks_.find(transport_id);
  if (it == get_raw_events_callbacks_.end())
    return;
  v8::Local<v8::Value> callback_args[] = {V8ValueConverter::Create()->ToV8Value(
      raw_events.get(), context()->v8_context())};
  context()->SafeCallFunction(v8::Local<v8::Function>::New(isolate, it->second),
                              base::size(callback_args), callback_args);
  get_raw_events_callbacks_.erase(it);
}

void CastStreamingNativeHandler::CallGetStatsCallback(
    int transport_id,
    std::unique_ptr<base::DictionaryValue> stats) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  auto it = get_stats_callbacks_.find(transport_id);
  if (it == get_stats_callbacks_.end())
    return;

  v8::Local<v8::Value> callback_args[] = {V8ValueConverter::Create()->ToV8Value(
      stats.get(), context()->v8_context())};
  context()->SafeCallFunction(v8::Local<v8::Function>::New(isolate, it->second),
                              base::size(callback_args), callback_args);
  get_stats_callbacks_.erase(it);
}

CastRtpStream* CastStreamingNativeHandler::GetRtpStreamOrThrow(
    int transport_id) const {
  auto iter = rtp_stream_map_.find(transport_id);
  if (iter != rtp_stream_map_.end())
    return iter->second.get();
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  isolate->ThrowException(v8::Exception::RangeError(
      v8::String::NewFromUtf8(isolate, kRtpStreamNotFound,
                              v8::NewStringType::kNormal)
          .ToLocalChecked()));
  return NULL;
}

CastUdpTransport* CastStreamingNativeHandler::GetUdpTransportOrThrow(
    int transport_id) const {
  auto iter = udp_transport_map_.find(transport_id);
  if (iter != udp_transport_map_.end())
    return iter->second.get();
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  isolate->ThrowException(v8::Exception::RangeError(
      v8::String::NewFromUtf8(isolate, kUdpTransportNotFound,
                              v8::NewStringType::kNormal)
          .ToLocalChecked()));
  return NULL;
}

bool CastStreamingNativeHandler::IPEndPointFromArg(
    v8::Isolate* isolate,
    const v8::Local<v8::Value>& arg,
    net::IPEndPoint* ip_endpoint) const {
  std::unique_ptr<base::Value> destination_value =
      V8ValueConverter::Create()->FromV8Value(arg, context()->v8_context());
  if (!destination_value) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kInvalidAesIvMask,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return false;
  }
  std::unique_ptr<IPEndPoint> destination =
      IPEndPoint::FromValue(*destination_value);
  if (!destination) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kInvalidDestination,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return false;
  }
  net::IPAddress ip;
  if (!ip.AssignFromIPLiteral(destination->address)) {
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, kInvalidDestination,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return false;
  }
  *ip_endpoint = net::IPEndPoint(ip, destination->port);
  return true;
}

void CastStreamingNativeHandler::CallReceiverErrorCallback(
    v8::CopyablePersistentTraits<v8::Function>::CopyablePersistent function,
    const std::string& error_message) {
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  v8::Local<v8::Value> arg =
      v8::String::NewFromUtf8(isolate, error_message.data(),
                              v8::NewStringType::kNormal, error_message.size())
          .ToLocalChecked();
  context()->SafeCallFunction(v8::Local<v8::Function>::New(isolate, function),
                              1, &arg);
}

}  // namespace extensions
