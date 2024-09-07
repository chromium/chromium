// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sharing_hub/preview_view.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
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

constexpr int kInterItemPadding = 4;

}  // namespace

SharingHubBubbleViewImpl::SharingHubBubbleViewImpl(
    views::View* anchor_view,
    share::ShareAttempt attempt,
    SharingHubBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view,
                                    attempt.web_contents.get(),
                                    /*autosize=*/true),
      attempt_(attempt) {
  DCHECK(anchor_view);
  DCHECK(controller);

  SetAccessibleTitle(l10n_util::GetStringUTF16(IDS_SHARING_HUB_TOOLTIP));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  RegisterWindowClosingCallback(base::BindOnce(
      &SharingHubBubbleViewImpl::OnWindowClosing, base::Unretained(this)));
  SetEnableArrowKeyTraversal(true);
  SetShowCloseButton(false);
  SetShowTitle(false);

  controller_ = controller->GetWeakPtr();
}

SharingHubBubbleViewImpl::~SharingHubBubbleViewImpl() = default;

void SharingHubBubbleViewImpl::Hide() {
  OnWindowClosing();
  CloseBubble();
}

void SharingHubBubbleViewImpl::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();
  if (GetWidget()) {
    set_color(GetColorProvider()->GetColor(ui::kColorMenuBackground));
  }
}

void SharingHubBubbleViewImpl::OnActionSelected(
    SharingHubBubbleActionButton* button) {
  if (!controller_)
    return;

  // The announcement has to happen here rather than in the button itself: the
  // button doesn't know whether controller_ will be null, so it doesn't know
  // whether the action will actually happen.
  const SharingHubAction& action = button->action_info();
  if (action.announcement_id) {
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(action.announcement_id));
  }
  controller_->OnActionSelected(action);

  Hide();
}

void SharingHubBubbleViewImpl::Init() {
  const int kPadding = 8;
  set_margins(gfx::Insets::TLBR(kPadding, 0, kPadding, 0));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kInterItemPadding));

  AddChildView(std::make_unique<PreviewView>(attempt_));
  AddChildView(std::make_unique<views::Separator>());

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kActionButtonHeight * kMaximumButtons);
  scroll_view_->SetBackgroundThemeColorId(ui::kColorMenuBackground);

  PopulateScrollView(controller_->GetFirstPartyActions());
}

void SharingHubBubbleViewImpl::PopulateScrollView(
    const std::vector<SharingHubAction>& first_party_actions) {
  auto* action_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  action_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kInterItemPadding));

  for (const auto& action : first_party_actions) {
    auto* view = action_list_view->AddChildView(
        std::make_unique<SharingHubBubbleActionButton>(this, action));
    view->SetGroup(kActionButtonGroup);
  }
}

void SharingHubBubbleViewImpl::OnWindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

BEGIN_METADATA(SharingHubBubbleViewImpl)
END_METADATA

}  // namespace sharing_hub
