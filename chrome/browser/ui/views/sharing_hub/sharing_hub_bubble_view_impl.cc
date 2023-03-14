// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "chrome/browser/share/share_features.h"
#include "chrome/browser/share/share_metrics.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/sharing_hub/preview_view.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_action_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
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
    : LocationBarBubbleDelegateView(anchor_view, attempt.web_contents.get()),
      attempt_(attempt) {
  DCHECK(anchor_view);
  DCHECK(controller);

  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetEnableArrowKeyTraversal(true);

  controller_ = controller->GetWeakPtr();
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
  if (show_time_) {
    share::RecordSharingHubTimeToShow(base::Time::Now() - *show_time_);
    show_time_ = absl::nullopt;
  }
}

void SharingHubBubbleViewImpl::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();
  if (GetWidget()) {
    set_color(GetColorProvider()->GetColor(ui::kColorMenuBackground));
    if (share_link_label_) {
      share_link_label_->SetBackgroundColor(
          GetColorProvider()->GetColor(ui::kColorMenuBackground));
      share_link_label_->SetEnabledColor(
          GetColorProvider()->GetColor(ui::kColorMenuItemForeground));
    }
  }
}

void SharingHubBubbleViewImpl::Show(DisplayReason reason) {
  show_time_ = base::Time::Now();
  ShowForReason(reason);
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

const views::View* SharingHubBubbleViewImpl::GetButtonContainerForTesting()
    const {
  return scroll_view_->contents();
}

void SharingHubBubbleViewImpl::Init() {
  const int kPadding = 8;
  set_margins(gfx::Insets::TLBR(kPadding, 0, kPadding, 0));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kInterItemPadding));
  if (controller_->ShouldUsePreview()) {
    auto* preview = AddChildView(std::make_unique<PreviewView>(
        attempt_, share::GetDesktopSharePreviewVariant()));
    preview->TakeCallbackSubscription(
        controller_->RegisterPreviewImageChangedCallback(base::BindRepeating(
            &PreviewView::OnImageChanged, base::Unretained(preview))));
    AddChildView(std::make_unique<views::Separator>());
  }

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

  MaybeSizeToContents();
  Layout();
}

void SharingHubBubbleViewImpl::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

}  // namespace sharing_hub
