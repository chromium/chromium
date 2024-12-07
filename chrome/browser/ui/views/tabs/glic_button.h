// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

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

  // TabStripControlsButton:
  void NotifyClick(const ui::Event& event) final;

 private:
  // Tab strip that contains this button.
  // TODO(crbug.com/382768227): Remove DanglingUntriaged.
  raw_ptr<TabStripController, AcrossTasksDanglingUntriaged>
      tab_strip_controller_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
