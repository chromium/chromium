// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/tabs/hover_tab_selector.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "components/prefs/pref_change_registrar.h"

class Browser;
class BrowserNonClientFrameView;
class Tab;
struct TabRendererData;

namespace content {
class WebContents;
}

namespace ui {
class ListSelectionModel;
}

// An implementation of TabStripController that sources data from the
// WebContentses in a TabStripModel.
class BrowserTabStripController : public TabStripController,
                                  public TabStripModelObserver {
 public:
  BrowserTabStripController(TabStripModel* model, BrowserView* browser_view);
  ~BrowserTabStripController() override;

  void InitFromModel(TabStrip* tabstrip);

  TabStripModel* model() const { return model_; }

  bool IsCommandEnabledForTab(TabStripModel::ContextMenuCommand command_id,
                              Tab* tab) const;
  void ExecuteCommandForTab(TabStripModel::ContextMenuCommand command_id,
                            Tab* tab);
  bool IsTabPinned(Tab* tab) const;

  // TabStripController implementation:
  const ui::ListSelectionModel& GetSelectionModel() const override;
  int GetCount() const override;
  bool IsValidIndex(int model_index) const override;
  bool IsActiveTab(int model_index) const override;
  int GetActiveIndex() const override;
  bool IsTabSelected(int model_index) const override;
  bool IsTabPinned(int model_index) const override;
  void SelectTab(int model_index) override;
  void ExtendSelectionTo(int model_index) override;
  void ToggleSelected(int model_index) override;
  void AddSelectionFromAnchorTo(int model_index) override;
  void CloseTab(int model_index, CloseTabSource source) override;
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
  SkColor GetTabBackgroundColor(TabState active) const override;
  SkColor GetTabForegroundColor(TabState state) const override;
  int GetTabBackgroundResourceId(
      BrowserNonClientFrameView::ActiveState active_state,
      bool* has_custom_image) const override;
  base::string16 GetAccessibleTabName(const Tab* tab) const override;
  Profile* GetProfile() const override;

  // TabStripModelObserver implementation:
  void TabInsertedAt(TabStripModel* tab_strip_model,
                     content::WebContents* contents,
                     int model_index,
                     bool is_active) override;
  void TabDetachedAt(content::WebContents* contents,
                     int model_index,
                     bool was_active) override;
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override;
  void TabSelectionChanged(TabStripModel* tab_strip_model,
                           const ui::ListSelectionModel& old_model) override;
  void TabMoved(content::WebContents* contents,
                int from_model_index,
                int to_model_index) override;
  void TabChangedAt(content::WebContents* contents,
                    int model_index,
                    TabChangeType change_type) override;
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents,
                     int model_index) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int model_index) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int model_index) override;
  void SetTabNeedsAttentionAt(int index, bool attention) override;

  const Browser* browser() const { return browser_view_->browser(); }

 private:
  class TabContextMenuContents;

  // The context in which TabRendererDataFromModel is being called.
  enum TabStatus {
    NEW_TAB,
    EXISTING_TAB
  };

  BrowserNonClientFrameView* GetFrameView();
  const BrowserNonClientFrameView* GetFrameView() const;

  // Returns the TabRendererData for the specified tab.
  TabRendererData TabRendererDataFromModel(content::WebContents* contents,
                                           int model_index,
                                           TabStatus tab_status);

  // Invokes tabstrip_->SetTabData.
  void SetTabDataAt(content::WebContents* web_contents, int model_index);

  void StartHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id,
      Tab* tab);
  void StopHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id,
      Tab* tab);

  // Adds a tab.
  void AddTab(content::WebContents* contents, int index, bool is_active);

  // Resets the tabstrips stacked layout (true or false) from prefs.
  void UpdateStackedLayout();

  TabStripModel* model_;

  TabStrip* tabstrip_;

  BrowserView* browser_view_;

  // If non-NULL it means we're showing a menu for the tab.
  std::unique_ptr<TabContextMenuContents> context_menu_contents_;

  // Helper for performing tab selection as a result of dragging over a tab.
  HoverTabSelector hover_tab_selector_;

  // Forces the tabs to use the regular (non-immersive) style and the
  // top-of-window views to be revealed when the user is dragging |tabstrip|'s
  // tabs.
  std::unique_ptr<ImmersiveRevealedLock> immersive_reveal_lock_;

  PrefChangeRegistrar local_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabStripController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
