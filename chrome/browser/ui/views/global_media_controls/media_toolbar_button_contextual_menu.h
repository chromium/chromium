// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTEXTUAL_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTEXTUAL_MENU_H_

#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
namespace global_media_controls {
class MediaItemManager;
}

// The contextual menu of the media toolbar button has two items, both of which
// are related to Cast. So this class should be instantiated only when
// GlobalMediaControlsCastStartStop is enabled.
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
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  void ToggleShowOtherSessions();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Opens the Cast feedback page.
  void ReportIssue();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  const raw_ptr<Browser, DanglingUntriaged> browser_;
  const raw_ptr<global_media_controls::MediaItemManager, DanglingUntriaged>
      item_manager_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTEXTUAL_MENU_H_
