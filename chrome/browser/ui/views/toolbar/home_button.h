// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class Browser;

class HomeButton : public ToolbarButton {
 public:
  HomeButton(views::ButtonListener* listener, Browser* browser);
  HomeButton(const HomeButton&) = delete;
  HomeButton& operator=(const HomeButton&) = delete;
  ~HomeButton() override;

 private:
  // ToolbarButton:
  const char* GetClassName() const override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

  Browser* const browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_HOME_BUTTON_H_
