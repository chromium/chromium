// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_about_this_site_content_view.h"

#include <vector>

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

PageInfoAboutThisSiteContentView::PageInfoAboutThisSiteContentView(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate)
    : presenter_(presenter), ui_delegate_(ui_delegate) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON)));

  info_ = ui_delegate_->GetAboutThisSiteInfo();
  auto* label = AddChildView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(info_->entity_description()),
      views::style::CONTEXT_LABEL));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  AddChildView(CreateSourceLabel(info_));

  // TODO(crbug.com/1250653): Add first indexed information if available.
}

PageInfoAboutThisSiteContentView::~PageInfoAboutThisSiteContentView() = default;

std::unique_ptr<views::View>
PageInfoAboutThisSiteContentView::CreateSourceLabel(
    const absl::optional<page_info::proto::SiteInfo> info) {
  auto source_label = std::make_unique<views::StyledLabel>();

  // TODO(crbug.com/1250653): Use actual strings.
  std::vector<std::u16string> subst;
  subst.push_back(u"From ");
  subst.push_back(base::UTF8ToUTF16(info->source_name()));

  std::vector<size_t> offsets;
  std::u16string text =
      base::ReplaceStringPlaceholders(u"$1 $2", subst, &offsets);
  source_label->SetText(text);
  gfx::Range details_range(offsets[1], text.length());

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
  ui_delegate_->AboutThisSiteSourceClicked(GURL(info_->source_url()), event);
}
