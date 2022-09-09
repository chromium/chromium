// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/confirm_bubble_model.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

ConfirmBubbleModel::ConfirmBubbleModel() = default;
ConfirmBubbleModel::~ConfirmBubbleModel() = default;

std::u16string ConfirmBubbleModel::GetButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      (button == ui::DIALOG_BUTTON_OK) ? IDS_OK : IDS_CANCEL);
}

void ConfirmBubbleModel::Accept() {}

void ConfirmBubbleModel::Cancel() {}

std::u16string ConfirmBubbleModel::GetLinkText() const {
  return std::u16string();
}

GURL ConfirmBubbleModel::GetHelpPageURL() const {
  return GURL();
}

void ConfirmBubbleModel::OpenHelpPage() {}
