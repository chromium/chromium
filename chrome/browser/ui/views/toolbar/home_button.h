// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

class Browser;

class HomeButton : public ToolbarButton {
 public:
  METADATA_HEADER(HomeButton);

  explicit HomeButton(PressedCallback callback = PressedCallback(),
                      Browser* browser = nullptr);
  HomeButton(const HomeButton&) = delete;
  HomeButton& operator=(const HomeButton&) = delete;
  ~HomeButton() override;

  // ToolbarButton:
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  ui::mojom::DragOperation OnPerformDrop(
      const ui::DropTargetEvent& event) override;

 private:
  Browser* const browser_;
};

BEGIN_VIEW_BUILDER(/* no export */, HomeButton, ToolbarButton)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, HomeButton)

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_
