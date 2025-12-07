// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_

#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;
class TabStripController;
class TabStrip;

// TabSearchButton should leverage the look and feel of the existing
// NewTabButton for sizing and appropriate theming. This class updates the
// NewTabButton with the appropriate icon and will be used to anchor the
// Tab Search bubble.
class TabSearchButton : public TabStripControlButton {
  METADATA_HEADER(TabSearchButton, TabStripControlButton)

 public:
  TabSearchButton(TabStripController* tab_strip_controller,
                  BrowserWindowInterface* browser_window_interface,
                  Edge fixed_flat_edge,
                  Edge animated_flat_edge,
                  TabStrip* tab_strip);
  TabSearchButton(const TabSearchButton&) = delete;
  TabSearchButton& operator=(const TabSearchButton&) = delete;
  ~TabSearchButton() override;

  // TabStripControlsButton:
  void NotifyClick(const ui::Event& event) final;

 protected:
  int GetCornerRadius() const override;
  int GetFlatCornerRadius() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
