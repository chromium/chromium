// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/common/buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_window_controller.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

class TabStripController;

namespace glic {

// GlicButton should leverage the look and feel of the existing
// TabSearchButton for sizing and appropriate theming.
//
// TODO(iwells): If this button moves outside of c/b/ui/views/tabs, rename to
// GlicTabStripButton.
class GlicButton : public TabStripControlButton {
  METADATA_HEADER(GlicButton, TabStripControlButton)

 public:
  explicit GlicButton(TabStripController* tab_strip_controller);
  GlicButton(const GlicButton&) = delete;
  GlicButton& operator=(const GlicButton&) = delete;
  ~GlicButton() override;

  // Triggers the UI programmatically.
  void ToggleUI();

  void SetDropToAttachIndicator(bool indicate);

  // GetBoundsInScreen() gives a rect with some padding that extends beyond the
  // visible edges of the button. This function returns a rect without that
  // padding.
  gfx::Rect GetBoundsWithInset() const;

 private:
  // Tab strip that contains this button.
  // TODO(crbug.com/382768227): Remove DanglingUntriaged.
  raw_ptr<TabStripController, AcrossTasksDanglingUntriaged>
      tab_strip_controller_;

#if BUILDFLAG(ENABLE_GLIC)
  // Used to observe glic panel state to update button icon.
  class GlicPanelStateObserver;
  std::unique_ptr<GlicPanelStateObserver> glic_panel_state_observer_;
#endif  // BUILDFLAG(ENABLE_GLIC)
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
