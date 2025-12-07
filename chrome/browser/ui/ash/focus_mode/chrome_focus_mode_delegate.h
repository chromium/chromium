// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_FOCUS_MODE_CHROME_FOCUS_MODE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_FOCUS_MODE_CHROME_FOCUS_MODE_DELEGATE_H_

#include "ash/system/focus_mode/focus_mode_delegate.h"

class ChromeFocusModeDelegate : public ash::FocusModeDelegate {
 public:
  ChromeFocusModeDelegate();
  ChromeFocusModeDelegate(const ChromeFocusModeDelegate&) = delete;
  ChromeFocusModeDelegate& operator=(const ChromeFocusModeDelegate&) = delete;
  ~ChromeFocusModeDelegate() override;

  // ash::FocusModeDelegate
  std::unique_ptr<ash::youtube_music::YouTubeMusicClient>
  CreateYouTubeMusicClient(const AccountId& account_id,
                           const std::string& device_id) override;
  const std::string& GetLocale() override;
  bool IsMinorUser() override;
};

#endif  // CHROME_BROWSER_UI_ASH_FOCUS_MODE_CHROME_FOCUS_MODE_DELEGATE_H_
