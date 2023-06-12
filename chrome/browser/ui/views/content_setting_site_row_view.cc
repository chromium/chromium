// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_site_row_view.h"

#include <memory>
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

ContentSettingSiteRowView::~ContentSettingSiteRowView() = default;

ContentSettingSiteRowView::ContentSettingSiteRowView(
    const net::SchemefulSite& site,
    bool allowed,
    ToggleCallback toggle_callback)
    : site_(site), toggle_callback_(toggle_callback) {
  SetLayoutManager(std::make_unique<views::FlexLayout>());

  auto title = url_formatter::FormatUrlForSecurityDisplay(
      site.GetURL(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  auto* title_label = AddChildView(std::make_unique<views::Label>(title));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  toggle_button_ = AddChildView(std::make_unique<views::ToggleButton>(
      base::BindRepeating(&ContentSettingSiteRowView::OnToggleButtonPressed,
                          base::Unretained(this))));
  toggle_button_->SetIsOn(allowed);
  toggle_button_->SetAccessibleName(
      l10n_util::GetStringFUTF16(IDS_PAGE_INFO_SELECTOR_TOOLTIP, title));
}

void ContentSettingSiteRowView::OnToggleButtonPressed() {
  toggle_callback_.Run(site_, toggle_button_->GetIsOn());
}

BEGIN_METADATA(ContentSettingSiteRowView, views::View) END_METADATA
