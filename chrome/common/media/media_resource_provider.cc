// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/common/media/media_resource_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int MediaMessageIdToGrdId(media::MessageId message_id) {
  switch (message_id) {
    case media::DEFAULT_AUDIO_DEVICE_NAME:
      return IDS_DEFAULT_AUDIO_DEVICE_NAME;
#if defined(OS_WIN)
    case media::COMMUNICATIONS_AUDIO_DEVICE_NAME:
      return IDS_COMMUNICATIONS_AUDIO_DEVICE_NAME;
#endif
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

base::string16 ChromeMediaLocalizedStringProvider(media::MessageId message_id) {
  return l10n_util::GetStringUTF16(MediaMessageIdToGrdId(message_id));
}
