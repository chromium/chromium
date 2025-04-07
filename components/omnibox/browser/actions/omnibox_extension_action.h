// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_EXTENSION_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_EXTENSION_ACTION_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/omnibox/browser/actions/omnibox_action.h"

// An Omnibox action added to an unscoped mode suggestion created by an
// extension.
class OmniboxExtensionAction : public OmniboxAction {
 public:
  OmniboxExtensionAction(const std::u16string& label,
                         const std::u16string& tooltip,
                         base::RepeatingClosure on_action_executed,
                         gfx::Image icon);

  // OmniboxAction:
  void Execute(ExecutionContext& context) const override;
  OmniboxActionId ActionId() const override;
  gfx::Image GetIconImage() const override;

 protected:
  ~OmniboxExtensionAction() override;

 private:
  base::RepeatingClosure on_action_executed_;
  gfx::Image icon_image_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_EXTENSION_ACTION_H_
