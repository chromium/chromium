// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
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
  return true;
}

std::u16string SharingHubBubbleViewImpl::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void SharingHubBubbleViewImpl::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void SharingHubBubbleViewImpl::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
}

void SharingHubBubbleViewImpl::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void SharingHubBubbleViewImpl::OnActionSelected(
    SharingHubBubbleActionButton* button) {
  if (!controller_)
    return;

  controller_->OnActionSelected(button->action_command_id(),
                                button->action_is_first_party());
  Hide();
}

const views::View* SharingHubBubbleViewImpl::GetButtonContainerForTesting()
    const {
  return scroll_view_->contents();
}

void SharingHubBubbleViewImpl::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kActionButtonHeight * kMaximumButtons);

  PopulateScrollView(controller_->GetActions());
}

void SharingHubBubbleViewImpl::PopulateScrollView(
    const std::vector<SharingHubAction>& actions) {
  auto* action_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  action_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  bool separator_added = false;
  for (const auto& action : actions) {
    if (!separator_added && !action.is_first_party) {
      action_list_view->AddChildView(GetSeparator());
      separator_added = true;
    }

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
