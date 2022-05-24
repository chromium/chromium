// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/observer_list_types.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"

namespace content {
class WebContents;
}  // namespace content

// Abstract interface for interactions with the Assistant side panel.
// Whoever owns an instance of it is responsible for destroying it
// and therefore effectively removing its entry from the unified side panel.
// The `WebContents` provided during creation must always outlive
// implementations of this interface.
class AssistantSidePanelCoordinator : public AssistantDisplayDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the side panel is hidden.
    virtual void OnHidden() {}
  };

  // If not already registered, registers an Assistant side panel entry in the
  // specified `WebContents` and returns and instance of
  // AssistantSidePanelCoordinator. Otherwise returns `nullptr`.
  static std::unique_ptr<AssistantSidePanelCoordinator> Create(
      content::WebContents* web_contents);

  // Returns `true` if a side panel entry is shown and `false` otherwise.
  virtual bool Shown() = 0;

  // Add an observer to the assistant side panel. Useful for listening to the
  // side panel being hidden.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer of the assistant side panel.
  virtual void RemoveObserver(Observer* observer) = 0;
};
#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_H_
