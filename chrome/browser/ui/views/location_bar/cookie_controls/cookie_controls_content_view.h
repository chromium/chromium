// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"

class RichControlsContainerView;

namespace views {
class Label;
class ToggleButton;
class ImageView;
}  // namespace views

// Content view used to display the cookie Controls.
class CookieControlsContentView : public views::View {
 public:
  METADATA_HEADER(CookieControlsContentView);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTitle);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDescription);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFeedbackButton);
  CookieControlsContentView();

  ~CookieControlsContentView() override;

  virtual void UpdateContentLabels(const std::u16string& title,
                                   const std::u16string& description);
  virtual void SetContentLabelsVisible(bool visible);

  virtual void SetToggleIsOn(bool is_on);
  virtual void SetToggleIcon(const gfx::VectorIcon& icon);

  virtual void SetToggleVisible(bool visible);
  virtual void SetToggleLabel(const std::u16string& label);
  virtual void SetEnforcedIcon(const gfx::VectorIcon& icon,
                               const std::u16string& tooltip);
  virtual void SetEnforcedIconVisible(bool visible);

  virtual void SetFeedbackSectionVisibility(bool visible);

  base::CallbackListSubscription RegisterToggleButtonPressedCallback(
      base::RepeatingCallback<void(bool)> callback);
  base::CallbackListSubscription RegisterFeedbackButtonPressedCallback(
      base::RepeatingClosureList::CallbackType callback);

 protected:
  gfx::Size CalculatePreferredSize() const override;

 private:
  friend class CookieControlsContentViewUnitTest;

  void NotifyToggleButtonPressedCallback();
  void NotifyFeedbackButtonPressedCallback();

  void AddContentLabels();
  void AddToggleRow();
  void AddFeedbackSection();
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<RichControlsContainerView> toggle_row_ = nullptr;
  raw_ptr<views::Label> toggle_label_ = nullptr;
  raw_ptr<views::ToggleButton> toggle_button_ = nullptr;
  raw_ptr<views::ImageView> enforced_icon_ = nullptr;
  raw_ptr<views::View> feedback_section_ = nullptr;

  base::RepeatingCallbackList<void(bool)> toggle_button_callback_list_;
  base::RepeatingClosureList feedback_button_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_
