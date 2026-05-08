// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CROSS_DEVICE_TAB_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CROSS_DEVICE_TAB_ACTION_H_

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

class CrossDeviceTabAction : public OmniboxAction {
 public:
  CrossDeviceTabAction();

  void RecordActionShown(size_t position, bool executed) const override;
  OmniboxActionId ActionId() const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

 private:
  ~CrossDeviceTabAction() override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CROSS_DEVICE_TAB_ACTION_H_
