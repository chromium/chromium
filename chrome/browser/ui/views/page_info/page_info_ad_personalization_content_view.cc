// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_ad_personalization_content_view.h"

#include <vector>

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
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
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate)
    : presenter_(presenter), ui_delegate_(ui_delegate) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  info_container_ = AddChildView(std::make_unique<views::View>());
  info_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  AddChildView(PageInfoViewFactory::CreateSeparator());
  // TODO(olesiamarukhno): Use correct strings.
  AddChildView(std::make_unique<PageInfoHoverButton>(
      base::BindRepeating(
          [](PageInfoAdPersonalizationContentView* view) {
            view->presenter_->RecordPageInfoAction(
                PageInfo::PageInfoAction::
                    PAGE_INFO_AD_PERSONALIZATION_SETTINGS_OPENED);
            view->ui_delegate_->ShowPrivacySandboxSettings();
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

void PageInfoAdPersonalizationContentView::SetAdPersonalizationInfo(
    const AdPersonalizationInfo& info) {
  if (!info.has_joined_user_to_interest_group)
    return;

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const auto button_insets =
      layout_provider->GetInsetsMetric(INSETS_PAGE_INFO_HOVER_BUTTON);

  // TODO(olesiamarukhno): Show different strings based on info.
  // TODO(olesiamarukhno): Use actual strings.
  auto* description_label =
      info_container_->AddChildView(std::make_unique<views::Label>(
          u"Duis ligula nisl, volutpat non est id, molestie cursus mauris. "
          u"Vestibulum iaculis, urna a finibus.",
          views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetProperty(views::kMarginsKey, button_insets);

  PreferredSizeChanged();
}
