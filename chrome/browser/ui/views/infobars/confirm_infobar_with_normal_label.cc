// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar_with_normal_label.h"

#include <utility>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"

ConfirmInfoBarWithNormalLabel::ConfirmInfoBarWithNormalLabel(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {
  auto* delegate_ptr = GetDelegate();
  auto label = CreateLabel(delegate_ptr->GetMessageText());
  label->SetElideBehavior(delegate_ptr->GetMessageElideBehavior());
  label_ = AssignMessageLabel(std::move(label));
}

ConfirmInfoBarWithNormalLabel::~ConfirmInfoBarWithNormalLabel() = default;

views::Label* ConfirmInfoBarWithNormalLabel::label_for_testing() {
  return label_;
}

BEGIN_METADATA(ConfirmInfoBarWithNormalLabel)
END_METADATA
