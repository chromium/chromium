// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/overflow_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

OverflowButton::OverflowButton() {
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_OVERFLOW_BUTTON));
  SetVectorIcon(kOverflowChevronIcon);
}

OverflowButton::~OverflowButton() = default;

BEGIN_METADATA(OverflowButton, ToolbarButton)
END_METADATA
