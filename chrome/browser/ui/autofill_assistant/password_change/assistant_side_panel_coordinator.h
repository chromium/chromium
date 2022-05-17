// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_H_

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

class SidePanelEntryObserver;

// Abstract interface for interactions with the Assistant side panel.
// Whoever owns an instance of it is responsible for destroying it
// and therefore effectively removing its entry from the unified side panel.
// The |WebContents| provided during creation must always outlive
// implementations of this interface.
class AssistantSidePanelCoordinator {
 public:
  // If not already registered, registers an Assistant Side Panel entry in the
  // specified |webContents| and returns and instance of
  // AssistantSidePanelCoordinator. Otherwise returns nullptr
  static std::unique_ptr<AssistantSidePanelCoordinator> Create(
      content::WebContents* web_contents);

  virtual ~AssistantSidePanelCoordinator() = default;

  // Returns true if a side panel entry is shown false otherwise.
  virtual bool Shown() = 0;

  // Sets the assistant side panel view.
  // This method takes ownership of the view, returning a pointer to it
  // which can be used for later updates.
  virtual views::View* SetView(std::unique_ptr<views::View> view) = 0;

  // Gets the current view rendered in the side panel. Returns null if the side
  // panel is hidden or no view has been set.
  virtual views::View* GetView() = 0;

  // Removes the current view rendered in the side panel and destroys it.
  virtual void RemoveView() = 0;

  // Add an observer to the assistant side panel. Useful for listening to the
  // side panel being hidden.
  virtual void AddObserver(SidePanelEntryObserver* observer) = 0;
};
#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_H_
