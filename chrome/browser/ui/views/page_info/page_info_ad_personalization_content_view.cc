// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_ad_personalization_content_view.h"

#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"

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
  AddChildView(std::make_unique<RichHoverButton>(
      base::BindRepeating(
          [](PageInfoAdPersonalizationContentView* view) {
            view->presenter_->RecordPageInfoAction(
                PageInfo::PageInfoAction::
                    PAGE_INFO_AD_PERSONALIZATION_SETTINGS_OPENED);
            view->ui_delegate_->ShowPrivacySandboxAdPersonalization();
          },
          this),
      PageInfoViewFactory::GetSiteSettingsIcon(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_AD_PERSONALIZATION_SUBPAGE_MANAGE_BUTTON),
      std::u16string(),
      /*tooltip_text=*/std::u16string(), std::u16string(),
      PageInfoViewFactory::GetLaunchIcon()));

  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoAdPersonalizationContentView::~PageInfoAdPersonalizationContentView() =
    default;

void PageInfoAdPersonalizationContentView::SetAdPersonalizationInfo(
    const AdPersonalizationInfo& info) {
  // The whole section shouldn't be visible when info is empty.
  DCHECK(!info.is_empty());

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const auto button_insets =
      layout_provider->GetInsetsMetric(INSETS_PAGE_INFO_HOVER_BUTTON);

  int message_id;
  if (info.has_joined_user_to_interest_group && !info.accessed_topics.empty()) {
    message_id =
        IDS_PAGE_INFO_AD_PERSONALIZATION_TOPICS_AND_INTEREST_GROUP_DESCRIPTION;
  } else if (info.has_joined_user_to_interest_group) {
    message_id = IDS_PAGE_INFO_AD_PERSONALIZATION_INTEREST_GROUP_DESCRIPTION;
  } else {
    message_id = IDS_PAGE_INFO_AD_PERSONALIZATION_TOPICS_DESCRIPTION;
  }
  auto* description_label =
      info_container_->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(message_id), views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY));
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetProperty(views::kMarginsKey, button_insets);

  if (!info.accessed_topics.empty()) {
    std::vector<std::u16string> topic_names;
    for (const auto& topic : info.accessed_topics) {
      topic_names.push_back(topic.GetLocalizedRepresentation());
    }
    auto* topic_label =
        info_container_->AddChildView(std::make_unique<views::Label>(
            base::JoinString(topic_names, u"\n"), views::style::CONTEXT_LABEL,
            views::style::STYLE_PRIMARY));
    topic_label->SetMultiLine(true);
    topic_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    topic_label->SetProperty(views::kMarginsKey, button_insets);
  }

  PreferredSizeChanged();
}
