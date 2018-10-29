// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "ui/base/models/list_selection_model.h"

class FakeBaseTabStripController : public TabStripController {
 public:
  FakeBaseTabStripController();
  ~FakeBaseTabStripController() override;

  void AddTab(int index, bool is_active);
  void AddPinnedTab(int index, bool is_active);
  void RemoveTab(int index);

  ui::ListSelectionModel* selection_model() { return &selection_model_; }

  void set_tab_strip(TabStrip* tab_strip) { tab_strip_ = tab_strip; }

  // TabStripController overrides:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  int GetCount() const override;
  bool IsValidIndex(int index) const override;
  bool IsActiveTab(int index) const override;
  int GetActiveIndex() const override;
  bool IsTabSelected(int index) const override;
  bool IsTabPinned(int index) const override;
  void SelectTab(int index) override;
  void ExtendSelectionTo(int index) override;
  void ToggleSelected(int index) override;
  void AddSelectionFromAnchorTo(int index) override;
  void CloseTab(int index, CloseTabSource source) override;
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override;
  int HasAvailableDragActions() const override;
  void OnDropIndexUpdate(int index, bool drop_before) override;
  bool IsCompatibleWith(TabStrip* other) const override;
  NewTabButtonPosition GetNewTabButtonPosition() const override;
  void CreateNewTab() override;
  void CreateNewTabWithLocation(const base::string16& loc) override;
  void StackedLayoutMaybeChanged() override;
  bool IsSingleTabModeAvailable() override;
  bool ShouldDrawStrokes() const override;
  void OnStartedDraggingTabs() override;
  void OnStoppedDraggingTabs() override;
  bool IsFrameCondensed() const override;
  bool HasVisibleBackgroundTabShapes() const override;
  bool EverHasVisibleBackgroundTabShapes() const override;
  SkColor GetFrameColor() const override;
  SkColor GetToolbarTopSeparatorColor() const override;
  SkColor GetTabBackgroundColor(TabState state) const override;
  SkColor GetTabForegroundColor(TabState state) const override;
  int GetTabBackgroundResourceId(
      BrowserNonClientFrameView::ActiveState active_state,
      bool* has_custom_image) const override;
  base::string16 GetAccessibleTabName(const Tab* tab) const override;
  Profile* GetProfile() const override;

 private:
  void SetActiveIndex(int new_index);

  TabStrip* tab_strip_ = nullptr;

  int num_tabs_ = 0;
  int active_index_ = -1;

  ui::ListSelectionModel selection_model_;

  DISALLOW_COPY_AND_ASSIGN(FakeBaseTabStripController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FAKE_BASE_TAB_STRIP_CONTROLLER_H_
