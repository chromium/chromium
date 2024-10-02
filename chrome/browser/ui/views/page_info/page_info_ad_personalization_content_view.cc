// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_ad_personalization_content_view.h"

#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"

PageInfoAdPersonalizationContentView::PageInfoAdPersonalizationContentView(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate)
    : presenter_(presenter), ui_delegate_(ui_delegate) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const auto button_insets =
      layout_provider->GetInsetsMetric(INSETS_PAGE_INFO_HOVER_BUTTON);
  const int vertical_distance =
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);

  SetOrientation(views::LayoutOrientation::kVertical);

  info_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  info_container_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  info_container_->SetInsideBorderInsets(button_insets);
  info_container_->SetBetweenChildSpacing(vertical_distance);

  AddChildView(PageInfoViewFactory::CreateSeparator(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW)));
  auto* manage_ad_privacy_button =
      AddChildView(std::make_unique<RichHoverButton>(
          base::BindRepeating(
              [](PageInfoAdPersonalizationContentView* view) {
                view->presenter_->RecordPageInfoAction(
                    page_info::PAGE_INFO_AD_PERSONALIZATION_SETTINGS_OPENED);
                view->ui_delegate_->ShowPrivacySandboxSettings();
              },
              this),
          PageInfoViewFactory::GetSiteSettingsIcon(),
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_AD_PRIVACY_SUBPAGE_MANAGE_BUTTON),
          std::u16string(),
          /*tooltip_text=*/std::u16string(), std::u16string(),
          PageInfoViewFactory::GetLaunchIcon()));
  manage_ad_privacy_button->title()->SetTextStyle(
      views::style::STYLE_BODY_3_MEDIUM);
  manage_ad_privacy_button->title()->SetEnabledColorId(
      kColorPageInfoForeground);

  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoAdPersonalizationContentView::~PageInfoAdPersonalizationContentView() =
    default;

void PageInfoAdPersonalizationContentView::SetAdPersonalizationInfo(
    const AdPersonalizationInfo& info) {
  // The whole section shouldn't be visible when info is empty.
  DCHECK(!info.is_empty());

  int message_id;
  if (info.has_joined_user_to_interest_group && !info.accessed_topics.empty()) {
    message_id = IDS_PAGE_INFO_AD_PRIVACY_TOPICS_AND_FLEDGE_DESCRIPTION;
  } else if (info.has_joined_user_to_interest_group) {
    message_id = IDS_PAGE_INFO_AD_PRIVACY_FLEDGE_DESCRIPTION;
  } else {
    message_id = IDS_PAGE_INFO_AD_PRIVACY_TOPICS_DESCRIPTION;
  }
  auto* description_label =
      info_container_->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(message_id), views::style::CONTEXT_LABEL,
          views::style::STYLE_BODY_3));
  description_label->SetMultiLine(true);
  description_label->SetEnabledColorId(kColorPageInfoForeground);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // TODO(crbug.com/40244046): Figure out why without additional horizontal
  // margin the size is being calculated incorrectly and the topics labels are
  // being cut off.
  auto label_margin = gfx::Insets::VH(0, 1);
  description_label->SetProperty(views::kMarginsKey, label_margin);

  int label_width = PageInfoViewFactory::kMinBubbleWidth -
                    info_container_->GetInsideBorderInsets().width() -
                    label_margin.width();
  description_label->SizeToFit(label_width);

  if (!info.accessed_topics.empty()) {
    std::vector<std::u16string> topic_names;
    for (const auto& topic : info.accessed_topics) {
      auto* topic_label =
          info_container_->AddChildView(std::make_unique<views::Label>(
              topic.GetLocalizedRepresentation(), views::style::CONTEXT_LABEL,
              views::style::STYLE_BODY_4));
      topic_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      topic_label->SetEnabledColorId(kColorPageInfoSubtitleForeground);
    }
  }

  PreferredSizeChanged();
}
