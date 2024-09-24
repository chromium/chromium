// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_RELAUNCH_CHROME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_RELAUNCH_CHROME_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/relaunch_chrome_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Bubble prompting user to relaunch Chrome.
class RelaunchChromeView : public PasswordBubbleViewBase {
  METADATA_HEADER(RelaunchChromeView, PasswordBubbleViewBase)

 public:
  RelaunchChromeView(content::WebContents* web_contents,
                     views::View* anchor_view,
                     PrefService* prefs);
  ~RelaunchChromeView() override;

 private:
  // PasswordBubbleViewBase:
  RelaunchChromeBubbleController* GetController() override;
  const RelaunchChromeBubbleController* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  RelaunchChromeBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_RELAUNCH_CHROME_VIEW_H_
