// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/text_with_controls_view.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class RichControlsContainerView;
namespace views {
class Label;
class ToggleButton;
class ImageView;
}  // namespace views

// Content view used to display the cookie Controls.
class CookieControlsContentView : public views::View {
  METADATA_HEADER(CookieControlsContentView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTitle);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDescription);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleLabel);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kThirdPartyCookiesLabel);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFeedbackButton);
  explicit CookieControlsContentView(bool has_act_features);

  ~CookieControlsContentView() override;

  virtual void UpdateContentLabels(const std::u16string& title,
                                   const std::u16string& description);
  virtual void SetContentLabelsVisible(bool visible);

  virtual void SetToggleIsOn(bool is_on);
  virtual void SetToggleIcon(const gfx::VectorIcon& icon);

  virtual void SetToggleVisible(bool visible);
  virtual void SetCookiesLabel(const std::u16string& label);
  virtual void SetEnforcedIcon(const gfx::VectorIcon& icon,
                               const std::u16string& tooltip);

  virtual void SetEnforcedIconVisible(bool visible);

  virtual void SetFeedbackSectionVisibility(bool visible);

  base::CallbackListSubscription RegisterToggleButtonPressedCallback(
      base::RepeatingCallback<void(bool)> callback);
  base::CallbackListSubscription RegisterFeedbackButtonPressedCallback(
      base::RepeatingClosureList::CallbackType callback);

  void AddFeatureRow(content_settings::TrackingProtectionFeature feature,
                     bool protections_on);
  void AddManagedSectionForEnforcement(CookieControlsEnforcement enforcement);
  void SetManagedSeparatorVisible(bool visible);
  void SetManagedSectionVisible(bool visible);

  void PreferredSizeChanged() override;

 protected:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  friend class CookieControlsContentViewUnitTest;
  friend class CookieControlsContentViewTrackingProtectionUnitTest;

  void NotifyToggleButtonPressedCallback();
  void NotifyFeedbackButtonPressedCallback();

  // Used for 3PC-only UI.
  void AddContentLabels();
  void AddToggleRow();
  void AddFeedbackSection();
  raw_ptr<RichControlsContainerView> cookies_row_ = nullptr;
  raw_ptr<views::View> feedback_section_ = nullptr;
  raw_ptr<views::View> label_wrapper_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<views::Label> cookies_label_ = nullptr;
  raw_ptr<views::ImageView> enforced_icon_ = nullptr;

  // Used for ACT features UI.
  bool has_act_features_ = false;
  void AddDescriptionRow();
  const ui::ElementIdentifier GetFeatureIdentifier(
      content_settings::TrackingProtectionFeatureType feature_type);

  raw_ptr<TextWithControlsView> description_row_ = nullptr;
  raw_ptr<views::ToggleButton> toggle_button_ = nullptr;
  raw_ptr<views::View> managed_separator_ = nullptr;
  raw_ptr<views::View> managed_section_ = nullptr;
  base::RepeatingCallbackList<void(bool)> toggle_button_callback_list_;
  base::RepeatingClosureList feedback_button_callback_list_;

  // Used for testing.
  raw_ptr<views::Label> managed_title_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_CONTENT_VIEW_H_
