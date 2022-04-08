// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/preview_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace sharing_hub {

namespace {
// These values are all directly from the Figma redlines. See
// https://crbug.com/1314486.
constexpr gfx::Size kImageSize{32, 32};
constexpr gfx::Insets kInteriorMargin = gfx::Insets::VH(12, 8);
constexpr gfx::Insets kDefaultMargin = gfx::Insets::VH(0, 8);
}  // namespace

// This view uses two nested FlexLayouts, a horizontal outer one and a vertical
// inner one, to create a composite layout where the icon is aligned with the
// title and URL taken together. The resulting View tree looks like this:
//   PreviewView [FlexLayout, horizontal]
//     ImageView
//     View [FlexLayout, vertical]
//       Label (title)
//       Label (URL)
PreviewView::PreviewView(std::u16string title, GURL url, ui::ImageModel image) {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kInteriorMargin)
      .SetDefault(views::kMarginsKey, kDefaultMargin)
      .SetCollapseMargins(true);

  image_ = AddChildView(std::make_unique<views::ImageView>(image));
  image_->SetPreferredSize(kImageSize);

  auto* labels_container = AddChildView(std::make_unique<views::View>());
  labels_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));
  auto* labels_layout =
      labels_container->SetLayoutManager(std::make_unique<views::FlexLayout>());
  labels_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));

  // TODO(ellyjones): These do not exactly match the redlines, which call for
  // 14pt Roboto specifically. We should probably update the redlines to not
  // use a hardcoded font, but we could also specify the font more explicitly
  // here.
  title_ = labels_container->AddChildView(std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_TITLE));
  url_ = labels_container->AddChildView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(url.spec()), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_HINT));
}

PreviewView::~PreviewView() = default;

void PreviewView::TakeCallbackSubscription(
    base::CallbackListSubscription subscription) {
  subscription_ = std::move(subscription);
}

void PreviewView::OnImageChanged(ui::ImageModel model) {
  image_->SetImage(model);
}

}  // namespace sharing_hub
