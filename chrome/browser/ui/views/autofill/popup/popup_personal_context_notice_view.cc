// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

PopupPersonalContextNoticeView::PopupPersonalContextNoticeView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(
      std::make_unique<views::Label>(u"PersonalContext Notice placeholder"));
}

PopupPersonalContextNoticeView::~PopupPersonalContextNoticeView() = default;

BEGIN_METADATA(PopupPersonalContextNoticeView)
END_METADATA

}  // namespace autofill
