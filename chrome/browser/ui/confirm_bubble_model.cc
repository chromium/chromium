// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/confirm_bubble_model.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

ConfirmBubbleModel::ConfirmBubbleModel() {}

ConfirmBubbleModel::~ConfirmBubbleModel() {}

int ConfirmBubbleModel::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 ConfirmBubbleModel::GetButtonLabel(BubbleButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_OK : IDS_CANCEL);
}

void ConfirmBubbleModel::Accept() {}

void ConfirmBubbleModel::Cancel() {}

base::string16 ConfirmBubbleModel::GetLinkText() const {
  return base::string16();
}

GURL ConfirmBubbleModel::GetHelpPageURL() const {
  return GURL();
}

void ConfirmBubbleModel::OpenHelpPage() {}
