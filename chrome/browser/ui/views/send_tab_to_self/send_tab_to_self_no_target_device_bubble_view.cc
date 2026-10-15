// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_no_target_device_bubble_view.h"

#include <memory>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/grit/branded_strings.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

namespace {

// Matches the vertical padding of `SendTabToSelfBubbleDeviceButton`, which is
// inherited from `views::HoverButton`.
constexpr int kAccountInfoTopMargin = 6;
constexpr int kAccountInfoBottomMargin = kAccountInfoTopMargin + 1;

constexpr int kAccountAvatarSize = 24;

// Creates a horizontal row displaying the circular avatar and email of the
// signed-in account.
std::unique_ptr<views::View> BuildAccountInfoView(
    base::WeakPtr<SendTabToSelfBubbleController> controller) {
  if (!controller) {
    return std::make_unique<views::View>();
  }

  auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets margins = provider->GetInsetsMetric(views::INSETS_DIALOG);
  margins.set_top(kAccountInfoTopMargin);
  margins.set_bottom(kAccountInfoBottomMargin);
  int between_child_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, margins,
      between_child_spacing));

  AccountInfo account = controller->GetSharingAccountInfo();
  CHECK(!account.IsEmpty());
  gfx::ImageSkia square_avatar =
      account.GetAvatarImage().value_or(gfx::Image()).AsImageSkia();
  // The color used in `circle_mask` is irrelevant as long as it is opaque; only
  // the alpha channel matters.
  gfx::ImageSkia circle_mask =
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          square_avatar.size().width() / 2, SK_ColorWHITE, gfx::ImageSkia());
  gfx::ImageSkia round_avatar =
      gfx::ImageSkiaOperations::CreateMaskedImage(square_avatar, circle_mask);
  auto* avatar_view =
      container->AddChildView(std::make_unique<views::ImageView>());
  avatar_view->SetImage(ui::ImageModel::FromImageSkia(
      gfx::ImageSkiaOperations::CreateResizedImage(
          round_avatar, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kAccountAvatarSize, kAccountAvatarSize))));

  container->AddChildView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(account.GetEmail()), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));

  return container;
}

}  // namespace

SendTabToSelfNoTargetDeviceBubbleView::SendTabToSelfNoTargetDeviceBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents)
    : SendTabToSelfBubbleView(anchor, web_contents) {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets::TLBR(provider->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                        0, 0, 0));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  InitLayout();
}

SendTabToSelfNoTargetDeviceBubbleView::
    ~SendTabToSelfNoTargetDeviceBubbleView() = default;

void SendTabToSelfNoTargetDeviceBubbleView::InitLayout() {
  auto* provider = ChromeLayoutProvider::Get();

  // Configure body text label.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  const int horizontal_padding =
      provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, horizontal_padding, /*bottom=*/0,
                        horizontal_padding));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  auto* account_view = AddChildView(BuildAccountInfoView(controller_));
  account_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(provider->GetDistanceMetric(
                          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                      0));
}

BEGIN_METADATA(SendTabToSelfNoTargetDeviceBubbleView)
END_METADATA

}  // namespace send_tab_to_self
