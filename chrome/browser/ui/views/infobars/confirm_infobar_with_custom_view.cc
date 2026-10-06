// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar_with_custom_view.h"

#include <utility>

#include "base/check.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

ConfirmInfoBarWithCustomView::ConfirmInfoBarWithCustomView(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate,
    std::unique_ptr<views::View> custom_message_view)
    : ConfirmInfoBar(std::move(delegate)) {
  CHECK(custom_message_view);
  custom_view_ = AssignMessageLabel(std::move(custom_message_view));
}

ConfirmInfoBarWithCustomView::~ConfirmInfoBarWithCustomView() = default;

BEGIN_METADATA(ConfirmInfoBarWithCustomView)
END_METADATA
