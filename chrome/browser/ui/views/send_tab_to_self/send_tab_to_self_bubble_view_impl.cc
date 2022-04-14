// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

namespace {

// The valid device button height.
constexpr int kDeviceButtonHeight = 56;
// Maximum number of buttons that are shown without scroll. If the device
// number is larger than kMaximumButtons, the bubble content will be
// scrollable.
constexpr int kMaximumButtons = 5;

// Used to group the device buttons together, which makes moving between them
// with arrow keys possible.
constexpr int kDeviceButtonGroup = 0;

// This sneakily matches the value of SendTabToSelfBubbleDeviceButton, which is
// inherited from views::HoverButton and isn't ever exposed.
constexpr int kManageDevicesLinkTopMargin = 6;
constexpr int kManageDevicesLinkBottomMargin = kManageDevicesLinkTopMargin + 1;

constexpr int kAccountAvatarSize = 24;

}  // namespace

SendTabToSelfBubbleViewImpl::SendTabToSelfBubbleViewImpl(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SendTabToSelfBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller->AsWeakPtr()) {
  DCHECK(controller_);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

SendTabToSelfBubbleViewImpl::~SendTabToSelfBubbleViewImpl() = default;

void SendTabToSelfBubbleViewImpl::Hide() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
  CloseBubble();
}

bool SendTabToSelfBubbleViewImpl::ShouldShowCloseButton() const {
  return true;
}

std::u16string SendTabToSelfBubbleViewImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SEND_TAB_TO_SELF);
}

void SendTabToSelfBubbleViewImpl::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void SendTabToSelfBubbleViewImpl::BackButtonPressed() {
  if (controller_) {
    controller_->OnBackButtonPressed();
    Hide();
  }
}

void SendTabToSelfBubbleViewImpl::DeviceButtonPressed(
    SendTabToSelfBubbleDeviceButton* device_button) {
  if (!controller_)
    return;

  controller_->OnDeviceSelected(device_button->device_guid());

  GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
      IDS_SEND_TAB_TO_SELF_SENDING_ANNOUNCE,
      base::UTF8ToUTF16(device_button->device_name())));

  Hide();
}

void SendTabToSelfBubbleViewImpl::OnManageDevicesClicked(
    const ui::Event& event) {
  controller_->OnManageDevicesClicked(event);
  Hide();
}

const views::View* SendTabToSelfBubbleViewImpl::GetButtonContainerForTesting()
    const {
  return scroll_view_->contents();
}

void SendTabToSelfBubbleViewImpl::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  const int top_margin = provider->GetDistanceMetric(
      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT);
  set_margins(gfx::Insets::TLBR(top_margin, 0, 0, 0));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  CreateHintTextLabel();
  CreateDevicesScrollView();

  AddChildView(std::make_unique<views::Separator>());
  CreateManageDevicesLink();
}

void SendTabToSelfBubbleViewImpl::AddedToWidget() {
  if (!controller_->show_back_button())
    return;

  // Adding a title view will replace the default title.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<sharing_hub::TitleWithBackButtonView>(
          base::BindRepeating(&SendTabToSelfBubbleViewImpl::BackButtonPressed,
                              base::Unretained(this)),
          GetWindowTitle()));
}

void SendTabToSelfBubbleViewImpl::CreateHintTextLabel() {
  views::View* container = AddChildView(std::make_unique<views::View>());
  auto* provider = ChromeLayoutProvider::Get();
  container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0,
                        provider->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        0));
  auto* container_layout =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::VH(0, provider->GetDistanceMetric(
                                 views::DISTANCE_BUTTON_HORIZONTAL_PADDING))));
  container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* description = container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_BUTTON_HINT_TEXT),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void SendTabToSelfBubbleViewImpl::CreateDevicesScrollView() {
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kDeviceButtonHeight * kMaximumButtons);

  auto* device_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  device_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (const TargetDeviceInfo& device : controller_->GetValidDevices()) {
    auto* view = device_list_view->AddChildView(
        std::make_unique<SendTabToSelfBubbleDeviceButton>(this, device));
    view->SetGroup(kDeviceButtonGroup);
  }

  if (!device_list_view->children().empty())
    SetInitiallyFocusedView(device_list_view->children()[0]);
}

void SendTabToSelfBubbleViewImpl::CreateManageDevicesLink() {
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetBackground(views::CreateThemedSolidBackground(
      ui::kColorMenuItemBackgroundHighlighted));

  auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets margins = provider->GetInsetsMetric(views::INSETS_DIALOG);
  margins.set_top(kManageDevicesLinkTopMargin);
  margins.set_bottom(kManageDevicesLinkBottomMargin);
  int between_child_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, margins,
      between_child_spacing));

  AccountInfo account = controller_->GetSharingAccountInfo();
  DCHECK(!account.IsEmpty());
  gfx::ImageSkia square_avatar = account.account_image.AsImageSkia();
  // The color used in `circle_mask` is irrelevant as long as it's opaque; only
  // the alpha channel matters.
  gfx::ImageSkia circle_mask =
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          square_avatar.size().width() / 2, SK_ColorWHITE, gfx::ImageSkia());
  gfx::ImageSkia round_avatar =
      gfx::ImageSkiaOperations::CreateMaskedImage(square_avatar, circle_mask);
  auto* avatar_view =
      container->AddChildView(std::make_unique<views::ImageView>());
  avatar_view->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
      round_avatar, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kAccountAvatarSize, kAccountAvatarSize)));

  auto* link_view =
      container->AddChildView(std::make_unique<views::StyledLabel>());
  link_view->SetDefaultTextStyle(views::style::STYLE_SECONDARY);

  // Only part of the string in |link_view| must be styled as a link and
  // clickable. This range is marked in the *.grd entry by the first 2
  // placeholders. This GetStringFUTF16() call replaces them with empty strings
  // (no-op) and saves the range in |offsets[0]| and |offsets[1]|.
  std::vector<size_t> offsets;
  link_view->SetText(l10n_util::GetStringFUTF16(
      IDS_SEND_TAB_TO_SELF_MANAGE_DEVICES_LINK,
      {std::u16string(), std::u16string(), base::UTF8ToUTF16(account.email)},
      &offsets));
  DCHECK_EQ(3u, offsets.size());
  // This object outlives its |link_view| child so base::Unretained() is safe.
  link_view->AddStyleRange(
      gfx::Range(offsets[0], offsets[1]),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &SendTabToSelfBubbleViewImpl::OnManageDevicesClicked,
          base::Unretained(this))));
}

}  // namespace send_tab_to_self
