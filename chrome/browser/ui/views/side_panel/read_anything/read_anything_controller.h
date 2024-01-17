// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_font_combobox.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "ui/base/models/combobox_model.h"

class Browser;

namespace content {
class WebContents;
}  // namespace content

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingController
//
//  A class that controls the Read Anything feature. This class does all of the
//  business logic of this feature and updates the model.
//  The controller is meant to be internal to the Read Anything feature and
//  classes outside this feature should not be making calls to it. The
//  coordinator or side panel controller is the external-facing API.
//  When the side panel entry is global, this class is owned by the
//  ReadAnythingCoordinator and has the same lifetime as the browser.
//  When the side panel entry is local. this class is owned by the
//  ReadAnythingSidePanelController and has the same lifetime as the associated
//  web contents.
//
class ReadAnythingController : public ReadAnythingToolbarView::Delegate,
                               public ReadAnythingFontCombobox::Delegate {
 public:
  ReadAnythingController(ReadAnythingModel* model, Browser* browser);
  ReadAnythingController(ReadAnythingModel* model,
                         content::WebContents* web_contents);
  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  virtual ~ReadAnythingController() = default;

 private:
  friend class ReadAnythingControllerTest;

  // ReadAnythingFontCombobox::Delegate:
  void OnFontChoiceChanged(int new_index) override;
  ReadAnythingFontModel* GetFontComboboxModel() override;

  // ReadAnythingToolbarView::Delegate:
  void OnFontSizeChanged(bool increase) override;
  void OnColorsChanged(int new_index) override;
  ReadAnythingMenuModel* GetColorsModel() override;
  void OnLineSpacingChanged(int new_index) override;
  ReadAnythingMenuModel* GetLineSpacingModel() override;
  void OnLetterSpacingChanged(int new_index) override;
  ReadAnythingMenuModel* GetLetterSpacingModel() override;
  void OnSystemThemeChanged() override;
  void OnLinksEnabledChanged(bool is_enabled) override;
  bool GetLinksEnabled() override;

  Profile* GetProfile();

  const raw_ptr<ReadAnythingModel> model_;
  const raw_ptr<content::WebContents> web_contents_;

  // Set when the ReadAnythingController is owned by a ReadAnythingCoordinator.
  // ReadAnythingCoordinator is a browser user data, so this pointer is always
  // valid.
  raw_ptr<Browser, DanglingUntriaged> browser_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
