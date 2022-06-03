// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace sharing_hub {

namespace {

// The valid action button height.
constexpr int kActionButtonHeight = 56;
// Maximum number of buttons that are shown without scrolling. If the number of
// actions is larger than kMaximumButtons, the bubble will be scrollable.
constexpr int kMaximumButtons = 10;

// Used to group the action buttons together, which makes moving between them
// with arrow keys possible.
constexpr int kActionButtonGroup = 0;

views::Separator* GetSeparator() {
  auto* separator = new views::Separator();
  separator->SetColor(gfx::kGoogleGrey300);
  const int kIndent = 16;
  const int kPadding = 8;
  constexpr auto kSeperatorBorder = gfx::Insets(kPadding, kIndent, 0, kIndent);
  separator->SetBorder(views::CreateEmptyBorder(kSeperatorBorder));
  return separator;
}
}  // namespace

SharingHubBubbleViewImpl::SharingHubBubbleViewImpl(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SharingHubBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetEnableArrowKeyTraversal(true);
  DCHECK(controller);
}

SharingHubBubbleViewImpl::~SharingHubBubbleViewImpl() = default;

void SharingHubBubbleViewImpl::Hide() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
  CloseBubble();
}

bool SharingHubBubbleViewImpl::ShouldShowCloseButton() const {
  return false;
}

bool SharingHubBubbleViewImpl::ShouldShowWindowTitle() const {
  return false;
}

void SharingHubBubbleViewImpl::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

std::u16string SharingHubBubbleViewImpl::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SHARING_HUB_TOOLTIP);
}

void SharingHubBubbleViewImpl::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
}

void SharingHubBubbleViewImpl::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();
  if (GetWidget()) {
    set_color(GetColorProvider()->GetColor(ui::kColorMenuBackground));
    share_link_label_->SetBackgroundColor(
        GetColorProvider()->GetColor(ui::kColorMenuBackground));
    share_link_label_->SetEnabledColor(
        GetColorProvider()->GetColor(ui::kColorMenuItemForeground));
  }
}

void SharingHubBubbleViewImpl::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void SharingHubBubbleViewImpl::OnActionSelected(
    SharingHubBubbleActionButton* button) {
  if (!controller_)
    return;

  controller_->OnActionSelected(button->action_command_id(),
                                button->action_is_first_party(),
                                button->action_name_for_metrics());

  Hide();
}

const views::View* SharingHubBubbleViewImpl::GetButtonContainerForTesting()
    const {
  return scroll_view_->contents();
}

void SharingHubBubbleViewImpl::Init() {
  const int kPadding = 8;
  set_margins(gfx::Insets(kPadding, 0, kPadding, 0));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kActionButtonHeight * kMaximumButtons);
  scroll_view_->SetBackgroundThemeColorId(ui::kColorMenuBackground);

  PopulateScrollView(controller_->GetFirstPartyActions(),
                     controller_->GetThirdPartyActions());
}

void SharingHubBubbleViewImpl::PopulateScrollView(
    const std::vector<SharingHubAction>& first_party_actions,
    const std::vector<SharingHubAction>& third_party_actions) {
  auto* action_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  action_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  for (const auto& action : first_party_actions) {
    auto* view = action_list_view->AddChildView(
        std::make_unique<SharingHubBubbleActionButton>(this, action));
    view->SetGroup(kActionButtonGroup);
  }

  action_list_view->AddChildView(GetSeparator());

  const int kLabelLineHeight = 22;
  const int kLabelLinePaddingTop = 8;
  const int kLabelLinePaddingBottom = 4;
  const int kIndent = 16;

  auto* share_link_label =
      new views::Label(l10n_util::GetStringUTF16(IDS_SHARING_HUB_SHARE_LABEL));
  share_link_label->SetFontList(gfx::FontList("GoogleSans, 13px"));
  share_link_label->SetLineHeight(kLabelLineHeight);
  share_link_label->SetMultiLine(true);
  share_link_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  share_link_label->SizeToFit(views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  constexpr auto kPrimaryIconBorder = gfx::Insets(
      /*top*/ kLabelLinePaddingTop,
      /*left*/ kIndent,
      /*bottom*/ kLabelLinePaddingBottom,
      /*right*/ kIndent);
  share_link_label->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  share_link_label_ = action_list_view->AddChildView(share_link_label);

  for (const auto& action : third_party_actions) {
    auto* view = action_list_view->AddChildView(
        std::make_unique<SharingHubBubbleActionButton>(this, action));
    view->SetGroup(kActionButtonGroup);
  }

  MaybeSizeToContents();
  Layout();
}

void SharingHubBubbleViewImpl::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

}  // namespace sharing_hub
