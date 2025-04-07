// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_extension_action.h"

#include <utility>

#include "base/base64.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

OmniboxExtensionAction::OmniboxExtensionAction(
    const std::u16string& label,
    const std::u16string& tooltip,
    base::RepeatingClosure on_action_executed,
    gfx::Image icon)
    : OmniboxAction(OmniboxAction::LabelStrings(
                        label,
                        tooltip,
                        l10n_util::GetStringUTF16(
                            IDS_ACC_OMNIBOX_ACTION_IN_EXTENSION_SUGGEST_SUFFIX),
                        tooltip),
                    GURL()),
      on_action_executed_(std::move(on_action_executed)),
      icon_image_(std::move(icon)) {
  CHECK(on_action_executed_);
}

OmniboxExtensionAction::~OmniboxExtensionAction() = default;

void OmniboxExtensionAction::Execute(ExecutionContext& context) const {
  on_action_executed_.Run();
}

OmniboxActionId OmniboxExtensionAction::ActionId() const {
  return OmniboxActionId::EXTENSION_ACTION;
}

gfx::Image OmniboxExtensionAction::GetIconImage() const {
  return icon_image_;
}
