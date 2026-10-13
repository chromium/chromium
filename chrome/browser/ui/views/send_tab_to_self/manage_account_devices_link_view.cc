// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/manage_account_devices_link_view.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace send_tab_to_self {

namespace {

// This sneakily matches the value of SendTabToSelfBubbleDeviceButton, which is
// inherited from views::HoverButton and isn't ever exposed.
constexpr int kManageDevicesLinkTopMargin = 6;
constexpr int kManageDevicesLinkBottomMargin = kManageDevicesLinkTopMargin + 1;

constexpr int kAccountAvatarSize = 24;

}  // namespace

std::unique_ptr<views::View> BuildManageAccountDevicesLinkView(
    base::WeakPtr<SendTabToSelfBubbleController> controller) {
  if (!controller) {
    return std::make_unique<views::View>();
  }

  auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets margins = provider->GetInsetsMetric(views::INSETS_DIALOG);
  margins.set_top(kManageDevicesLinkTopMargin);
  margins.set_bottom(kManageDevicesLinkBottomMargin);
  int between_child_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, margins,
      between_child_spacing));

  AccountInfo account = controller->GetSharingAccountInfo();
  DCHECK(!account.IsEmpty());
  gfx::ImageSkia square_avatar =
      account.GetAvatarImage().value_or(gfx::Image()).AsImageSkia();
  // The color used in `circle_mask` is irrelevant as long as it's opaque; only
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

  auto* link_view =
      container->AddChildView(std::make_unique<views::StyledLabel>());
  link_view->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  link_view->SetText(base::UTF8ToUTF16(account.GetEmail()));

  return container;
}

}  // namespace send_tab_to_self
