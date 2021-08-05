// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace base {
class ElapsedTimer;
}

class BrowserView;
class Profile;

class ChromeLabsButton : public ToolbarButton {
 public:
  METADATA_HEADER(ChromeLabsButton);
  explicit ChromeLabsButton(BrowserView* browser_view,
                            const ChromeLabsBubbleViewModel* model);
  ChromeLabsButton(const ChromeLabsButton&) = delete;
  ChromeLabsButton& operator=(const ChromeLabsButton&) = delete;
  ~ChromeLabsButton() override;

  static bool ShouldShowButton(const ChromeLabsBubbleViewModel* model,
                               Profile* profile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::ElapsedTimer* GetAshOwnerCheckTimer() {
    return ash_owner_check_timer_.get();
  }

  void SetShouldCircumventDeviceCheckForTesting(bool should_circumvent) {
    should_circumvent_device_check_for_testing_ = should_circumvent;
  }
#endif

 private:
  void ButtonPressed();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Measures elapsed between when IsOwnerAsync is called and the callback
  // passed into IsOwnerAsnc is called. The callback will be called after
  // ownership is established.
  std::unique_ptr<base::ElapsedTimer> ash_owner_check_timer_;
#endif

  BrowserView* browser_view_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_waiting_to_show = false;
  // Used to circumvent the IsRunningOnChromeOS() check in ash-chrome tests.
  bool should_circumvent_device_check_for_testing_ = false;
#endif

  const ChromeLabsBubbleViewModel* model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_
