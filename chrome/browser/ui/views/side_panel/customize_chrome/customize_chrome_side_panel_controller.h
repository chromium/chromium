// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"

class CustomizeChromeUI;
class SidePanelUI;

namespace tabs {
class TabInterface;
}  // namespace tabs

// Abstract base class exposed to NewTabPageUI. This only exists to allow
// mocking in tests.
class CustomizeChromeSidePanelControllerBase {
 public:
  virtual ~CustomizeChromeSidePanelControllerBase() = default;
  using StateChangedCallBack = base::RepeatingCallback<void(bool)>;
  virtual bool IsCustomizeChromeEntryShowing() const = 0;
  virtual void SetCallback(StateChangedCallBack callback) = 0;
  virtual void SetCustomizeChromeSidePanelVisible(
      bool visible,
      CustomizeChromeSection section) = 0;
};

// Responsible for implementing logic to create and register/deregister the
// customize chrome side panel.
class CustomizeChromeSidePanelController
    : public SidePanelEntryObserver,
      public CustomizeChromeSidePanelControllerBase {
 public:
  explicit CustomizeChromeSidePanelController(tabs::TabInterface* tab);
  CustomizeChromeSidePanelController(
      const CustomizeChromeSidePanelController&) = delete;
  CustomizeChromeSidePanelController& operator=(
      const CustomizeChromeSidePanelController&) = delete;
  ~CustomizeChromeSidePanelController() override;

  void CreateAndRegisterEntry();
  void DeregisterEntry();
  bool IsCustomizeChromeEntryAvailable() const;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // CustomizeChromeSidePanelControllerBase:
  bool IsCustomizeChromeEntryShowing() const override;
  void SetCallback(StateChangedCallBack callback) override;
  void SetCustomizeChromeSidePanelVisible(
      bool visible,
      CustomizeChromeSection section) override;

 private:
  // Creates view for side panel entry.
  std::unique_ptr<views::View> CreateCustomizeChromeWebView();

  SidePanelUI* GetSidePanelUI() const;

  const raw_ptr<tabs::TabInterface> tab_;
  base::WeakPtr<CustomizeChromeUI> customize_chrome_ui_;
  // Caches a request to scroll to a section in case the request happens before
  // the front-end is ready to receive the request.
  std::optional<CustomizeChromeSection> section_;
  StateChangedCallBack entry_state_changed_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
