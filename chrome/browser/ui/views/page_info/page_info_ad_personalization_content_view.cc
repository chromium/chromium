// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_ad_personalization_content_view.h"

#include <vector>

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

PageInfoAdPersonalizationContentView::PageInfoAdPersonalizationContentView(
    PageInfo* presenter)
    : presenter_(presenter) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const auto button_insets =
      layout_provider->GetInsetsMetric(INSETS_PAGE_INFO_HOVER_BUTTON);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  auto* info_container = AddChildView(std::make_unique<views::View>());
  info_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // TODO(olesiamarukhno): Use actual strings.
  auto* description_label =
      info_container->AddChildView(std::make_unique<views::Label>(
          u"Duis ligula nisl, volutpat non est id, molestie cursus mauris. "
          u"Vestibulum iaculis, urna a finibus.",
          views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetProperty(views::kMarginsKey, button_insets);

  AddChildView(PageInfoViewFactory::CreateSeparator());
  // TODO(olesiamarukhno): Use correct strings.
  AddChildView(std::make_unique<PageInfoHoverButton>(
      base::BindRepeating(
          [](PageInfoAdPersonalizationContentView* view) {
            // TODO(olesiamarukhno): Open settings.
          },
          this),
      PageInfoViewFactory::GetSiteSettingsIcon(),
      IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_MANAGE_BUTTON, std::u16string(), 0,
      /*tooltip_text=*/std::u16string(), std::u16string(),
      PageInfoViewFactory::GetLaunchIcon()));

  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoAdPersonalizationContentView::~PageInfoAdPersonalizationContentView() =
    default;
