// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_PAGE_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/tab_strip/tab_before_unload_tracker.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip.mojom.h"
#include "chrome/browser/ui/webui/tab_strip/thumbnail_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class Browser;
class TabStripUIEmbedder;

class TabStripPageHandler : public tab_strip::mojom::PageHandler,
                            public TabStripModelObserver,
                            public content::WebContentsDelegate,
                            public ThemeServiceObserver,
                            public ui::NativeThemeObserver {
 public:
  TabStripPageHandler(
      mojo::PendingReceiver<tab_strip::mojom::PageHandler> receiver,
      mojo::PendingRemote<tab_strip::mojom::Page> page,
      content::WebUI* web_ui,
      Browser* browser,
      TabStripUIEmbedder* embedder);
  TabStripPageHandler(const TabStripPageHandler&) = delete;
  TabStripPageHandler& operator=(const TabStripPageHandler&) = delete;
  ~TabStripPageHandler() override;

  void NotifyLayoutChanged();
  void NotifyReceivedKeyboardFocus();
  void NotifyContextMenuClosed();

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabGroupedStateChanged(std::optional<tab_groups::TabGroupId> group,
                              tabs::TabModel* tab,
                              int index) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int index) override;

  // content::WebContentsDelegate:
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  bool IsPrivileged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, CloseTab);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, GetGroupVisualData);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, GroupTab);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, MoveGroup);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, MoveGroupAcrossProfiles);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, MoveGroupAcrossWindows);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, MoveGroupMultipleTabs);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, MoveTab);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, MoveTabAcrossProfiles);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, MoveTabAcrossWindows);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest,
                           RemoveTabIfInvalidContextMenu);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, SwitchTab);
  FRIEND_TEST_ALL_PREFIXES(TabStripPageHandlerTest, UngroupTab);

  void OnLongPressTimer();
  tab_strip::mojom::TabPtr GetTabData(content::WebContents* contents,
                                      int index);
  tab_strip::mojom::TabGroupVisualDataPtr GetTabGroupData(TabGroup* group);
  void HandleThumbnailUpdate(content::WebContents* tab,
                             ThumbnailTracker::CompressedThumbnailData image);
  void OnTabCloseCancelled(content::WebContents* tab);
  void ReportTabDurationHistogram(const char* histogram_fragment,
                                  int tab_count,
                                  base::TimeDelta duration);
  gfx::ImageSkia ThemeFavicon(const gfx::ImageSkia& source,
                              bool active_tab_icon);

  // ThemeServiceObserver implementation.
  void OnThemeChanged() override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // tab_strip::mojom::PageHandler:
  void GetTabs(GetTabsCallback callback) override;
  void GetGroupVisualData(GetGroupVisualDataCallback callback) override;
  void CloseContainer() override;
  void CloseTab(int32_t tab_id, bool tab_was_swiped) override;
  void ShowEditDialogForGroup(const std::string& group_id,
                              int32_t location_x,
                              int32_t location_y,
                              int32_t width,
                              int32_t height) override;
  void ShowBackgroundContextMenu(int32_t location_x,
                                 int32_t location_y) override;
  void ShowTabContextMenu(int32_t tab_id,
                          int32_t location_x,
                          int32_t location_y) override;
  void GetLayout(GetLayoutCallback callback) override;
  void GroupTab(int32_t tab_id, const std::string& group_id) override;
  void UngroupTab(int32_t tab_id) override;
  void MoveGroup(const std::string& group_id, int32_t to_index) override;
  void MoveTab(int32_t tab_id, int32_t to_index) override;
  void SetThumbnailTracked(int32_t tab_id, bool thumbnail_tracked) override;
  void ReportTabActivationDuration(uint32_t duration_ms) override;
  void ReportTabDataReceivedDuration(uint32_t tab_count,
                                     uint32_t duration_ms) override;
  void ReportTabCreationDuration(uint32_t tab_count,
                                 uint32_t duration_ms) override;
  void ActivateTab(int32_t tab_id) override;

  mojo::Receiver<tab_strip::mojom::PageHandler> receiver_;
  mojo::Remote<tab_strip::mojom::Page> page_;

  const raw_ptr<content::WebUI> web_ui_;

  const raw_ptr<Browser> browser_;
  const raw_ptr<TabStripUIEmbedder> embedder_;
  ThumbnailTracker thumbnail_tracker_;
  tab_strip_ui::TabBeforeUnloadTracker tab_before_unload_tracker_;

  // Tracks whether we are currently handling a gesture scroll event sequence.
  bool handling_gesture_scroll_ = false;

  // A flag that tracks whether or not a scroll begin gesture event should
  // initiate a drag. This is used to ensure we start a drag only for event
  // streams intended to trigger a drag (See crbug.com/1204572).
  bool should_drag_on_gesture_scroll_ = false;

  // Determines whether to show the context menu after a tap gesture.
  const bool context_menu_after_tap_;

  // The point at which the initial gesture tap event occurred and at which the
  // drag will start.
  gfx::Point touch_drag_start_point_;

  // Timer that starts when a press gesture is encountered and runs for the
  // duration of a long press. The timer is stopped if the tap gesture is
  // interrupted (eg by a scroll start gesture).
  std::unique_ptr<base::RetainingOneShotTimer> long_press_timer_;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};

  base::WeakPtrFactory<TabStripPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_PAGE_HANDLER_H_
