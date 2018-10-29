// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/browser/cdm_message_filter_android.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "components/cdm/common/cdm_messages_android.h"
#include "content/public/browser/android/android_overlay_provider.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"

using media::MediaDrmBridge;
using media::SupportedCodecs;

namespace cdm {

const size_t kMaxKeySystemLength = 256;

template <typename CodecType>
struct CodecInfo {
  SupportedCodecs eme_codec;
  CodecType codec;
  const char* container_mime_type;
};

const CodecInfo<media::VideoCodec> kVideoCodecsToQuery[] = {
    {media::EME_CODEC_VP8, media::kCodecVP8, "video/webm"},
    // TODO(crbug.com/707127): Support query for VP9 profile 1/2/3 on Android.
    {media::EME_CODEC_VP9_PROFILE0, media::kCodecVP9, "video/webm"},
    {media::EME_CODEC_VP9_PROFILE0, media::kCodecVP9, "video/mp4"},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {media::EME_CODEC_AVC1, media::kCodecH264, "video/mp4"},
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    {media::EME_CODEC_HEVC, media::kCodecHEVC, "video/mp4"},
#endif
#if BUILDFLAG(ENABLE_DOLBY_VISION_DEMUXING)
    {media::EME_CODEC_DOLBY_VISION_AVC, media::kCodecDolbyVision, "video/mp4"},
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    {media::EME_CODEC_DOLBY_VISION_HEVC, media::kCodecDolbyVision, "video/mp4"},
#endif
#endif
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
};

const CodecInfo<media::AudioCodec> kAudioCodecsToQuery[] = {
    // FLAC is not supported. See https://crbug.com/747050 for details.
    // Vorbis is not supported. See http://crbug.com/710924 for details.
    {media::EME_CODEC_OPUS, media::kCodecOpus, "video/webm"},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    {media::EME_CODEC_AAC, media::kCodecAAC, "video/mp4"},
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    {media::EME_CODEC_AC3, media::kCodecAC3, "video/mp4"},
    {media::EME_CODEC_EAC3, media::kCodecEAC3, "video/mp4"},
#endif
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
};

static SupportedCodecs GetSupportedCodecs(
    const SupportedKeySystemRequest& request,
    bool is_secure) {
  const std::string& key_system = request.key_system;
  SupportedCodecs supported_codecs = media::EME_CODEC_NONE;

  for (const auto& info : kVideoCodecsToQuery) {
    if ((request.codecs & info.eme_codec) &&
        MediaDrmBridge::IsKeySystemSupportedWithType(
            key_system, info.container_mime_type) &&
        media::MediaCodecUtil::CanDecode(info.codec, is_secure)) {
      supported_codecs |= info.eme_codec;
    }
  }

  for (const auto& info : kAudioCodecsToQuery) {
    if ((request.codecs & info.eme_codec) &&
        MediaDrmBridge::IsKeySystemSupportedWithType(
            key_system, info.container_mime_type) &&
        media::MediaCodecUtil::CanDecode(info.codec)) {
      supported_codecs |= info.eme_codec;
    }
  }

  return supported_codecs;
}

CdmMessageFilterAndroid::CdmMessageFilterAndroid(
    bool can_persist_data,
    bool force_to_support_secure_codecs)
    : BrowserMessageFilter(EncryptedMediaMsgStart),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      can_persist_data_(can_persist_data),
      force_to_support_secure_codecs_(force_to_support_secure_codecs) {}

CdmMessageFilterAndroid::~CdmMessageFilterAndroid() {}

bool CdmMessageFilterAndroid::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CdmMessageFilterAndroid, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_QueryKeySystemSupport,
                        OnQueryKeySystemSupport)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_GetPlatformKeySystemNames,
                        OnGetPlatformKeySystemNames)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

base::TaskRunner* CdmMessageFilterAndroid::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  // Move the IPC handling to FILE thread as it is not very cheap.
  if (message.type() == ChromeViewHostMsg_QueryKeySystemSupport::ID)
    return task_runner_.get();

  return nullptr;
}

void CdmMessageFilterAndroid::OnQueryKeySystemSupport(
    const SupportedKeySystemRequest& request,
    SupportedKeySystemResponse* response) {
  if (!response) {
    NOTREACHED() << "NULL response pointer provided.";
    return;
  }

  if (request.key_system.size() > kMaxKeySystemLength) {
    NOTREACHED() << "Invalid key system: " << request.key_system;
    return;
  }

  if (!MediaDrmBridge::IsKeySystemSupported(request.key_system))
    return;

  // When using MediaDrm, we assume it'll always try to persist some data. If
  // |can_persist_data_| is false and MediaDrm were to persist data on the
  // Android system, we are somewhat violating the incognito assumption.
  // This cannot be used detect incognito mode easily because the result is the
  // same when |can_persist_data_| is false, and when user blocks the "protected
  // media identifier" permission prompt.
  if (!can_persist_data_)
    return;

  DCHECK(request.codecs & media::EME_CODEC_ALL) << "unrecognized codec";
  response->key_system = request.key_system;
  response->non_secure_codecs = GetSupportedCodecs(request, false);

  bool are_overlay_supported =
      content::AndroidOverlayProvider::GetInstance()->AreOverlaysSupported();
  bool use_android_overlay =
      base::FeatureList::IsEnabled(media::kUseAndroidOverlay);
  if (force_to_support_secure_codecs_ ||
      (are_overlay_supported && use_android_overlay)) {
    DVLOG(1) << "Rendering the output of secure codecs is supported!";
    response->secure_codecs = GetSupportedCodecs(request, true);
  }

  response->is_persistent_license_supported =
      MediaDrmBridge::IsPersistentLicenseTypeSupported(request.key_system);
}

void CdmMessageFilterAndroid::OnGetPlatformKeySystemNames(
    std::vector<std::string>* key_systems) {
  *key_systems = MediaDrmBridge::GetPlatformKeySystemNames();
}

}  // namespace cdm
