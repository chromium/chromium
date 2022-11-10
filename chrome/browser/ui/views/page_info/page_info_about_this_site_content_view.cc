// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_about_this_site_content_view.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/page_info/core/features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

using Orientation = views::BoxLayout::Orientation;

PageInfoAboutThisSiteContentView::PageInfoAboutThisSiteContentView(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate,
    const page_info::proto::SiteInfo& info)
    : presenter_(presenter), ui_delegate_(ui_delegate), info_(info) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(Orientation::kVertical));

  auto* info_container = AddChildView(std::make_unique<views::View>());
  info_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(Orientation::kVertical));
  info_container->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON)));
  info_container->AddChildView(CreateDescriptionLabel(info_));
  info_container->AddChildView(CreateSourceLabel(info_));

  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoAboutThisSiteContentView::~PageInfoAboutThisSiteContentView() = default;

std::unique_ptr<views::View>
PageInfoAboutThisSiteContentView::CreateDescriptionLabel(
    const page_info::proto::SiteInfo& info) {
  auto label = std::make_unique<views::Label>(
      base::UTF8ToUTF16(info_.description().description()),
      views::style::CONTEXT_LABEL);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

std::unique_ptr<views::View>
PageInfoAboutThisSiteContentView::CreateSourceLabel(
    const page_info::proto::SiteInfo& info) {
  auto source_label = std::make_unique<views::StyledLabel>();

  size_t offset;
  std::u16string source_name =
      base::UTF8ToUTF16(info.description().source().label());
  std::u16string text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_ABOUT_THIS_SITE_SUBPAGE_FROM_LABEL, source_name, &offset);

  source_label->SetText(text);
  gfx::Range details_range(offset, offset + source_name.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PageInfoAboutThisSiteContentView::SourceLinkClicked,
          base::Unretained(this)));
  source_label->AddStyleRange(details_range, link_style);

  return source_label;
}

void PageInfoAboutThisSiteContentView::SourceLinkClicked(
    const ui::Event& event) {
  presenter_->RecordPageInfoAction(
      PageInfo::PageInfoAction::PAGE_INFO_ABOUT_THIS_SITE_SOURCE_LINK_CLICKED);
  ui_delegate_->AboutThisSiteSourceClicked(
      GURL(info_.description().source().url()), event);
}
