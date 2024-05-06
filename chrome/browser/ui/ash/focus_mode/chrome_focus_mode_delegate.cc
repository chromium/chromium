// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/focus_mode/chrome_focus_mode_delegate.h"

ChromeFocusModeDelegate::ChromeFocusModeDelegate() = default;

ChromeFocusModeDelegate::~ChromeFocusModeDelegate() = default;

std::unique_ptr<ash::youtube_music::YoutubeMusicClient>
ChromeFocusModeDelegate::CreateYoutubeMusicClient() {
  // TODO(yongshun): Create and return the client.
  return nullptr;
}
