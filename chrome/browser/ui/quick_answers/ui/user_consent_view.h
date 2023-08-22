// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/editor_menu/utils/focus_search.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
class UserConsentView : public views::View {
 public:
  METADATA_HEADER(UserConsentView);

  static constexpr char kWidgetName[] = "UserConsentViewWidget";

  UserConsentView(const gfx::Rect& anchor_view_bounds,
                  const std::u16string& intent_type,
                  const std::u16string& intent_text,
                  base::WeakPtr<QuickAnswersUiController> controller);

  // Disallow copy and assign.
  UserConsentView(const UserConsentView&) = delete;
  UserConsentView& operator=(const UserConsentView&) = delete;

  ~UserConsentView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds,
      const std::u16string& intent_type,
      const std::u16string& intent_text,
      base::WeakPtr<QuickAnswersUiController> controller);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnFocus() override;
  void OnThemeChanged() override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void UpdateAnchorViewBounds(const gfx::Rect& anchor_view_bounds);

 private:
  void InitLayout();
  void InitContent();
  void InitButtonBar();
  void UpdateWidgetBounds();

  // FocusSearch::GetFocusableViewsCallback to poll currently focusable views.
  std::vector<views::View*> GetFocusableViews();

  // Cached bounds of the anchor this view is tied to.
  gfx::Rect anchor_view_bounds_;
  // Cached title text.
  std::u16string title_text_;

  chromeos::editor_menu::PreTargetHandler event_handler_;
  base::WeakPtr<QuickAnswersUiController> controller_;
  chromeos::editor_menu::FocusSearch focus_search_;

  // Owned by view hierarchy.
  raw_ptr<views::View> main_view_ = nullptr;
  raw_ptr<views::View> content_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> desc_ = nullptr;
  raw_ptr<views::LabelButton> no_thanks_button_ = nullptr;
  raw_ptr<views::LabelButton> allow_button_ = nullptr;
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_USER_CONSENT_VIEW_H_
