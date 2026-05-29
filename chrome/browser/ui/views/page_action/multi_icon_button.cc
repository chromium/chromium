// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/multi_icon_button.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {

namespace {
const int kAnchoredMessageIconSize = 20;
const int kAnchoredMessageMaxExpandButtonIcons = 3;
}  // namespace

MultiIconButton::MultiIconButton(PressedCallback callback)
    : views::Button(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kHorizontal,
                       gfx::Insets::VH(4, 8), -10))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetInstallFocusRingOnFocus(true);
  SetAccessibleName(u"Show details");
}

MultiIconButton::~MultiIconButton() = default;

views::View::Views MultiIconButton::GetChildrenInZOrder() {
  auto children = views::Button::GetChildrenInZOrder();
  std::reverse(children.begin(), children.end());
  return children;
}

void MultiIconButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  if (color_provider) {
    SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetColor(ui::kColorSysPrimaryContainer), 12));
    if (plus_more_label_) {
      plus_more_label_->SetEnabledColor(
          color_provider->GetColor(ui::kColorSysOnPrimaryContainer));
    }
  }
}

void MultiIconButton::Update(
    const std::vector<std::reference_wrapper<const ui::ImageModel>>& icons) {
  plus_more_label_ = nullptr;
  RemoveAllChildViews();
  int count = 0;
  for (const ui::ImageModel& icon : icons) {
    if (count < kAnchoredMessageMaxExpandButtonIcons) {
      if (!icon.IsEmpty()) {
        auto* icon_view = AddChildView(std::make_unique<views::ImageView>());
        icon_view->SetImage(icon);
        icon_view->SetImageSize(
            gfx::Size(kAnchoredMessageIconSize, kAnchoredMessageIconSize));
      }
    }
    count++;
  }
  if (count > kAnchoredMessageMaxExpandButtonIcons) {
    plus_more_label_ = AddChildView(std::make_unique<views::Label>(base::StrCat(
        {u"+",
         base::FormatNumber(count - kAnchoredMessageMaxExpandButtonIcons)})));
    plus_more_label_->SetTextStyle(views::style::STYLE_BODY_5);
    plus_more_label_->SetProperty(views::kMarginsKey,
                                  gfx::Insets::TLBR(0, 14, 0, 0));
  }
}

BEGIN_METADATA(MultiIconButton)
END_METADATA

}  // namespace page_actions
