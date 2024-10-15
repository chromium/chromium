// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_local_answer_header_view.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"

OmniboxLocalAnswerHeaderView::OmniboxLocalAnswerHeaderView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(25, 21, 2, 0), 8));
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  text_ = AddChildView(std::make_unique<views::Label>());
  text_->SetFontList(views::TypographyProvider::Get().GetFont(
      CONTEXT_OMNIBOX_POPUP, STYLE_SMALL));
}

void OmniboxLocalAnswerHeaderView::OnThemeChanged() {
  views::View::OnThemeChanged();
  icon_->SetImage(ui::ImageModel::FromVectorIcon(omnibox::kSummarizeAutoIcon,
                                                 kColorOmniboxResultsIcon, 20));
  text_->SetEnabledColor(
      GetColorProvider()->GetColor(kColorOmniboxResultsTextDimmed));
}

void OmniboxLocalAnswerHeaderView::SetText(const std::u16string& text) {
  text_->SetText(text);
}

BEGIN_METADATA(OmniboxLocalAnswerHeaderView)
END_METADATA
