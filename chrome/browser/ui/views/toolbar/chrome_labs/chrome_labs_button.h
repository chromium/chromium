// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/dot_indicator.h"

class BrowserView;

class ChromeLabsButton : public ToolbarButton {
  METADATA_HEADER(ChromeLabsButton, ToolbarButton)

 public:
  explicit ChromeLabsButton(BrowserView* browser_view,
                            const ChromeLabsModel* model);
  ChromeLabsButton(const ChromeLabsButton&) = delete;
  ChromeLabsButton& operator=(const ChromeLabsButton&) = delete;
  ~ChromeLabsButton() override;

  // ToolbarButton:
  void Layout(PassKey) override;

  void HideDotIndicator();

  void ButtonPressed();

  bool GetDotIndicatorVisibilityForTesting() const {
    return new_experiments_indicator_->GetVisible();
  }

 private:
  void UpdateDotIndicator();

  raw_ptr<BrowserView, DanglingUntriaged> browser_view_;

  raw_ptr<const ChromeLabsModel, AcrossTasksDanglingUntriaged> model_;

  raw_ptr<views::DotIndicator> new_experiments_indicator_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_CHROME_LABS_BUTTON_H_
