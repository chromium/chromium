// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

// Content view used to display the cookie Controls.
class CookieControlsContentView : public views::View {
 public:
  CookieControlsContentView();

  ~CookieControlsContentView() override;

  void UpdateContentLabels(const std::u16string& title,
                           const std::u16string& description);

  void SetFeedbackSectionVisibility(bool visible);

  base::CallbackListSubscription RegisterFeedbackButtonPressedCallback(
      base::RepeatingClosureList::CallbackType callback);

 private:
  void NotifyFeedbackButtonPressedCallback();

  void AddContentLabels();
  void AddFeedbackSection();
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<views::View> feedback_section_ = nullptr;

  base::RepeatingClosureList restart_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_
