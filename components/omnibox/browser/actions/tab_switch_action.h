// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_TAB_SWITCH_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_TAB_SWITCH_ACTION_H_

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "url/gurl.h"

class TabSwitchAction : public OmniboxAction {
 public:
  explicit TabSwitchAction(GURL url);

  void Execute(ExecutionContext& context) const override;

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

  OmniboxActionId ActionId() const override;

 protected:
  ~TabSwitchAction() override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_TAB_SWITCH_ACTION_H_
