// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTEXTUAL_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTEXTUAL_MENU_H_

#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

// The contextual menu of the media toolbar button has only one item, which is
// to open the Cast feedback page. So this class should be instantiated when:
//  (1) It is a Chrome branded build.
//  (2) GlobalMediaControlsCastStartStop is enabled.
class MediaToolbarButtonContextualMenu : public ui::SimpleMenuModel::Delegate {
 public:
  static std::unique_ptr<MediaToolbarButtonContextualMenu> Create(
      Browser* browser);
  explicit MediaToolbarButtonContextualMenu(Browser* browser);
  MediaToolbarButtonContextualMenu(const MediaToolbarButtonContextualMenu&) =
      delete;
  MediaToolbarButtonContextualMenu& operator=(
      const MediaToolbarButtonContextualMenu&) = delete;
  ~MediaToolbarButtonContextualMenu() override;

  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

 private:
  friend class MediaToolbarButtonContextualMenuTest;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Opens the Cast feedback page.
  void ReportIssue();

  const raw_ptr<Browser> browser_;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};
#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTEXTUAL_MENU_H_
