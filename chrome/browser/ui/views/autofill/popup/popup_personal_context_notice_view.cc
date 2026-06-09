// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

PopupPersonalContextNoticeView::PopupPersonalContextNoticeView(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : controller_(std::move(controller)), line_number_(line_number) {
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
  if (controller_) {
    // TODO(crbug.com/520201413): Add metrics to track the cases when
    // `RemoveSuggestion` returns false.
    controller_->RemoveSuggestion(
        line_number_,
        AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked);
  }
}

PopupPersonalContextNoticeView::~PopupPersonalContextNoticeView() = default;

BEGIN_METADATA(PopupPersonalContextNoticeView)
END_METADATA

}  // namespace autofill
