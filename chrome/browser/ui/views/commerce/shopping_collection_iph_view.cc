// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/shopping_collection_iph_view.h"

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"

namespace commerce {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kShoppingCollectionIPHViewId);

namespace {
const int kTitleBottomMargin = 8;
}  // namespace

ShoppingCollectionIphView::ShoppingCollectionIphView() {
  SetProperty(views::kElementIdentifierKey, kShoppingCollectionIPHViewId);
  std::unique_ptr<views::FlexLayout> layout =
      std::make_unique<views::FlexLayout>();
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetDefault(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred, true));
  SetLayoutManager(std::move(layout));

  std::u16string title_text =
      l10n_util::GetStringUTF16(IDS_SHOPPING_COLLECTION_IPH_TITLE);
  auto* title = AddChildView(std::make_unique<views::Label>(
      title_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_EMPHASIZED));
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  title->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(0, 0, kTitleBottomMargin, 0));

  const bool power_bookmarks_side_panel_enabled =
      base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel);
  const auto label_context = power_bookmarks_side_panel_enabled
                                 ? views::style::CONTEXT_LABEL
                                 : views::style::CONTEXT_DIALOG_BODY_TEXT;
  std::u16string body_text =
      l10n_util::GetStringUTF16(IDS_SHOPPING_COLLECTION_IPH_BODY);
  views::Label* body_label = AddChildView(std::make_unique<views::Label>(
      body_text, label_context, views::style::STYLE_SECONDARY));
  body_label->SetMultiLine(true);
  body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (power_bookmarks_side_panel_enabled) {
    body_label->SetFontList(body_label->font_list().DeriveWithSizeDelta(-1));
  }
}

ShoppingCollectionIphView::~ShoppingCollectionIphView() = default;

}  // namespace commerce
