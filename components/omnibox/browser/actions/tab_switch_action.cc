// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/tab_switch_action.h"

#include <numeric>

#include "build/build_config.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/window_open_disposition.h"

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

TabSwitchAction::TabSwitchAction(GURL url)
    : OmniboxAction(LabelStrings(IDS_OMNIBOX_TAB_SUGGEST_HINT,
                                 IDS_OMNIBOX_TAB_SUGGEST_HINT,
                                 IDS_ACC_TAB_SWITCH_SUFFIX,
                                 IDS_ACC_TAB_SWITCH_BUTTON),
                    std::move(url)) {}

TabSwitchAction::~TabSwitchAction() = default;

void TabSwitchAction::Execute(ExecutionContext& context) const {
  context.disposition_ = WindowOpenDisposition::SWITCH_TO_TAB;
  OpenURL(context, url_);
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& TabSwitchAction::GetVectorIcon() const {
  return omnibox::kSwitchCr2023Icon;
}
#endif

OmniboxActionId TabSwitchAction::ActionId() const {
  return OmniboxActionId::TAB_SWITCH;
}
