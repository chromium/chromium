// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/styled_label.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoMerchantTrustContentView,
                                      kElementIdForTesting);

PageInfoMerchantTrustContentView::PageInfoMerchantTrustContentView() {
  SetProperty(views::kElementIdentifierKey, kElementIdForTesting);
  SetOrientation(views::LayoutOrientation::kVertical);
  // TODO(crbug.com/378854730): Set up layout.

  AddChildView(CreateDescriptionLabel());
}

PageInfoMerchantTrustContentView::~PageInfoMerchantTrustContentView() = default;

std::unique_ptr<views::View>
PageInfoMerchantTrustContentView::CreateDescriptionLabel() {
  auto description_label = std::make_unique<views::StyledLabel>();

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  // The top and bottom margins should be the same as for buttons shown below.
  const auto button_insets = layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);

  description_label->SetProperty(views::kMarginsKey, button_insets);
  description_label->SetDefaultTextStyle(views::style::STYLE_BODY_3);
  description_label->SetDefaultEnabledColorId(kColorPageInfoSubtitleForeground);
  description_label->SizeToFit(PageInfoViewFactory::kMinBubbleWidth -
                               button_insets.width());

  size_t offset;
  std::u16string text_for_link =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_LEARN_MORE_LINK);
  description_label->SetText(l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_MERCHANT_TRUST_DESCRIPTION, text_for_link, &offset));

  gfx::Range link_range(offset, offset + text_for_link.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PageInfoMerchantTrustContentView::LearnMoreLinkClicked,
          base::Unretained(this)));
  // TODO(crbug.com/378854730): Add STYLE_LINK_4 and change the label text style
  // to STYLE_BODY_4.
  link_style.text_style = views::style::STYLE_LINK_3;
  description_label->AddStyleRange(link_range, link_style);
  return description_label;
}

void PageInfoMerchantTrustContentView::LearnMoreLinkClicked(
    const ui::Event& event) {
  // TODO(crbug.com/381405880): Open learn more link.
}
