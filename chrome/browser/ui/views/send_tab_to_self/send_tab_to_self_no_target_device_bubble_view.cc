// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_no_target_device_bubble_view.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/manage_account_devices_link_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

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
  auto* link_view = AddChildView(
      BuildManageAccountDevicesLinkView(/*show_link=*/false, controller_));
  link_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(provider->GetDistanceMetric(
                          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                      0));
}

BEGIN_METADATA(SendTabToSelfNoTargetDeviceBubbleView)
END_METADATA

}  // namespace send_tab_to_self
