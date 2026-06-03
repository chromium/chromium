// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

PopupPersonalContextNoticeView::PopupPersonalContextNoticeView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(
      std::make_unique<views::Label>(u"PersonalContext Notice placeholder"));

  // TODO(crbug.com/517520354): Add styling and strings.
  got_it_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PopupPersonalContextNoticeView::OnGotItButtonClicked,
                          base::Unretained(this)),
      u"OK"));
}

void PopupPersonalContextNoticeView::OnGotItButtonClicked() {
  // TODO(crbug.com/515651053): Connect to the backend logic.
}

PopupPersonalContextNoticeView::~PopupPersonalContextNoticeView() = default;

BEGIN_METADATA(PopupPersonalContextNoticeView)
END_METADATA

}  // namespace autofill
