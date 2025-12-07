// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/media_resource_provider.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "media/base/localized_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int MediaMessageIdToGrdId(media::MessageId message_id) {
  switch (message_id) {
    case media::DEFAULT_AUDIO_DEVICE_NAME:
      return IDS_DEFAULT_AUDIO_DEVICE_NAME;
#if BUILDFLAG(IS_WIN)
    case media::COMMUNICATIONS_AUDIO_DEVICE_NAME:
      return IDS_COMMUNICATIONS_AUDIO_DEVICE_NAME;
#endif
#if BUILDFLAG(IS_ANDROID)
    case media::GENERIC_AUDIO_DEVICE_NAME:
      return IDS_GENERIC_AUDIO_DEVICE_NAME;
    case media::INTERNAL_SPEAKER_AUDIO_DEVICE_NAME:
      return IDS_INTERNAL_SPEAKER_AUDIO_DEVICE_NAME;
    case media::INTERNAL_MIC_AUDIO_DEVICE_NAME:
      return IDS_INTERNAL_MIC_AUDIO_DEVICE_NAME;
    case media::WIRED_HEADPHONES_AUDIO_DEVICE_NAME:
      return IDS_WIRED_HEADPHONES_AUDIO_DEVICE_NAME;
    case media::BLUETOOTH_AUDIO_DEVICE_NAME:
      return IDS_BLUETOOTH_AUDIO_DEVICE_NAME;
    case media::USB_AUDIO_DEVICE_NAME:
      return IDS_USB_AUDIO_DEVICE_NAME;
    case media::HDMI_AUDIO_DEVICE_NAME:
      return IDS_HDMI_AUDIO_DEVICE_NAME;
#endif
  }
  NOTREACHED();
}

}  // namespace

std::u16string ChromeMediaLocalizedStringProvider(media::MessageId message_id) {
  return l10n_util::GetStringUTF16(MediaMessageIdToGrdId(message_id));
}
