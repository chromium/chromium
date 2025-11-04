// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_MAGIC_BOOST_USER_CONSENT_VIEW_H_
#define CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_MAGIC_BOOST_USER_CONSENT_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/editor_menu/utils/focus_search.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_view.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class LabelButton;
}  // namespace views

namespace quick_answers {

class MagicBoostUserConsentView : public chromeos::ReadWriteCardsView {
  METADATA_HEADER(MagicBoostUserConsentView, chromeos::ReadWriteCardsView)

 public:
  static constexpr char kWidgetName[] = "MagicBoostUserConsentViewWidget";

  // TODO(b/340628664): remove `read_write_cards_ui_controller` arg once we stop
  // extending `ReadWriteCardsView`.
  MagicBoostUserConsentView(
      IntentType intent_type,
      const std::u16string& intent_text,
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller);

  // Disallow copy and assign.
  MagicBoostUserConsentView(const MagicBoostUserConsentView&) = delete;
  MagicBoostUserConsentView& operator=(const MagicBoostUserConsentView&) =
      delete;

  ~MagicBoostUserConsentView() override;

  // chromeos::ReadWriteCardsView:
  void OnFocus() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void UpdateBoundsForQuickAnswers() override;

  void SetSettingsButtonPressed(views::Button::PressedCallback callback);
  void SetIntentButtonPressedCallback(views::Button::PressedCallback callback);

  std::u16string chip_label_for_testing();
  views::ImageButton* settings_button_for_testing() { return settings_button_; }

 private:
  // FocusSearch::GetFocusableViewsCallback to poll currently focusable views.
  std::vector<views::View*> GetFocusableViews();

  chromeos::editor_menu::FocusSearch focus_search_;
  raw_ptr<views::LabelButton> intent_chip_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   MagicBoostUserConsentView,
                   chromeos::ReadWriteCardsView)
VIEW_BUILDER_PROPERTY(views::Button::PressedCallback, SettingsButtonPressed)
VIEW_BUILDER_PROPERTY(views::Button::PressedCallback,
                      IntentButtonPressedCallback)
END_VIEW_BUILDER

}  // namespace quick_answers

DEFINE_VIEW_BUILDER(/* no export */, quick_answers::MagicBoostUserConsentView)

#endif  // CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_UI_MAGIC_BOOST_USER_CONSENT_VIEW_H_
