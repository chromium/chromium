// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"

#include <stddef.h>

#include "base/strings/string16.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

BubbleSyncPromoView::BubbleSyncPromoView(
    BubbleSyncPromoDelegate* delegate,
    signin_metrics::AccessPoint access_point,
    int link_text_resource_id,
    int message_text_resource_id)
    : StyledLabel(base::string16(), this), delegate_(delegate) {
  size_t offset = 0;
  base::string16 link_text = l10n_util::GetStringUTF16(link_text_resource_id);
  base::string16 promo_text =
      l10n_util::GetStringFUTF16(message_text_resource_id, link_text, &offset);
  SetText(promo_text);

  AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                views::StyledLabel::RangeStyleInfo::CreateForLink());

  views::StyledLabel::RangeStyleInfo promo_style;
  promo_style.text_style = views::style::STYLE_SECONDARY;
  gfx::Range before_link_range(0, offset);
  if (!before_link_range.is_empty())
    AddStyleRange(before_link_range, promo_style);
  gfx::Range after_link_range(offset + link_text.length(), promo_text.length());
  if (!after_link_range.is_empty())
    AddStyleRange(after_link_range, promo_style);

  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(access_point);
}

BubbleSyncPromoView::~BubbleSyncPromoView() {}

const char* BubbleSyncPromoView::GetClassName() const {
  return "BubbleSyncPromoView";
}

void BubbleSyncPromoView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  delegate_->OnEnableSync(AccountInfo(), false /* is_default_promo_account */);
}
