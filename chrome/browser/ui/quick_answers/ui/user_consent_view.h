// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/views/editor_menu/utils/focus_search.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

class QuickAnswersUiController;

namespace quick_answers {

// |intent_type| and |intent_text| are used to generate the consent title
// including predicted intent information. Fallback to title without intent
// information if any of these two strings are empty.
class UserConsentView : public chromeos::ReadWriteCardsView {
  METADATA_HEADER(UserConsentView, chromeos::ReadWriteCardsView)

 public:
  static constexpr char kWidgetName[] = "UserConsentViewWidget";

  // TODO(b/340628664): remove `read_write_cards_ui_controller` arg once we stop
  // extending `ReadWriteCardsView`.
  explicit UserConsentView(
      bool use_refreshed_design,
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller);

  // Disallow copy and assign.
  UserConsentView(const UserConsentView&) = delete;
  UserConsentView& operator=(const UserConsentView&) = delete;

  ~UserConsentView() override;

  // chromeos::ReadWriteCardsView:
  void OnFocus() override;
  void OnThemeChanged() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void UpdateBoundsForQuickAnswers() override;

  void SetNoThanksButtonPressed(views::Button::PressedCallback callback);
  void SetAllowButtonPressed(views::Button::PressedCallback callback);
  void SetIntentType(IntentType intent_type);
  void SetIntentText(const std::u16string& intent_text);

  views::LabelButton* allow_button_for_test() { return allow_button_; }
  views::LabelButton* no_thanks_button_for_test() { return no_thanks_button_; }

 private:
  void UpdateUiText();
  void UpdateIcon();

  // FocusSearch::GetFocusableViewsCallback to poll currently focusable views.
  std::vector<views::View*> GetFocusableViews();

  base::WeakPtr<QuickAnswersUiController> controller_;
  chromeos::editor_menu::FocusSearch focus_search_;

  IntentType intent_type_ = IntentType::kUnknown;
  std::u16string intent_text_;
  bool use_refreshed_design_ = false;

  // Owned by view hierarchy.
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<views::LabelButton> no_thanks_button_ = nullptr;
  raw_ptr<views::LabelButton> allow_button_ = nullptr;
  raw_ptr<views::ImageView> dictionary_intent_icon_ = nullptr;
  raw_ptr<views::ImageView> translation_intent_icon_ = nullptr;
  raw_ptr<views::ImageView> unit_intent_icon_ = nullptr;
  raw_ptr<views::ImageView> unknown_intent_icon_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   UserConsentView,
                   chromeos::ReadWriteCardsView)
VIEW_BUILDER_PROPERTY(views::Button::PressedCallback, NoThanksButtonPressed)
VIEW_BUILDER_PROPERTY(views::Button::PressedCallback, AllowButtonPressed)
VIEW_BUILDER_PROPERTY(IntentType, IntentType)
VIEW_BUILDER_PROPERTY(const std::u16string&, IntentText)
END_VIEW_BUILDER

}  // namespace quick_answers

DEFINE_VIEW_BUILDER(/* no export */, quick_answers::UserConsentView)

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
