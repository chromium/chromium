// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

inline constexpr char kClassicContextTypeHistogramPrefix[] =
    "Omnibox.AimEntrypoint.ClassicPopup.ContextualElement";
inline constexpr char kAimContextTypeHistogramPrefix[] =
    "Omnibox.AimEntrypoint.AimPopup.ContextualElement";

class FaviconService;
class OmniboxPopupFileSelector;
class OmniboxPopupUI;
class OmniboxEditModel;
class OmniboxController;

namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace ui {
class ImageModel;
}  // namespace ui

namespace omnibox {
enum ChromeAimToolsAndModels : int;
}  // namespace omnibox

namespace content {
class WebContents;
}  // namespace content

namespace contextual_search {
struct FileInfo;
}  // namespace contextual_search

enum class OmniboxPopupState;

// OmniboxContextMenuController creates and manages state for the context menu
// shown for the omnibox.
class OmniboxContextMenuController : public ui::SimpleMenuModel::Delegate {
 public:
  explicit OmniboxContextMenuController(OmniboxPopupFileSelector* file_selector,
                                        content::WebContents* web_contents);
  struct TabInfo {
    int tab_id;
    std::u16string title;
    GURL url;
    base::TimeTicks last_active;
  };

  OmniboxContextMenuController(const OmniboxContextMenuController&) = delete;
  OmniboxContextMenuController& operator=(const OmniboxContextMenuController&) =
      delete;

  ~OmniboxContextMenuController() override;

  ui::SimpleMenuModel* menu_model() { return menu_model_.get(); }

  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void AddTabContext(const TabInfo& tab_info);
  void UpdateSearchboxContext(
      std::optional<TabInfo> tab_info,
      std::optional<searchbox::mojom::ToolMode> tool_mode);

  // Tracks the context type.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ContextType)
  enum class ContextType {
    kTab = 0,
    kFile = 1,
    kImage = 2,
    kImageGen = 3,
    kDeepResearch = 4,
    kMaxValue = kDeepResearch,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:ContextType,//tools/metrics/histograms/metadata/omnibox/histograms.xml:ContextType)

 private:
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerTest,
                           IsCommandIdEnabledHelper_InitialState);
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerTest,
                           IsCommandIdEnabledHelper_ImageGenMode);
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerTest,
                           IsCommandIdEnabledHelper_WithImageFile);
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerTest,
                           IsCommandIdEnabledHelper_WithNonImageFile);
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerTest,
                           IsCommandIdEnabledHelper_MaxFiles);

  // Helper function for `IsCommandIdEnabled` exposing main logic to make
  // unit testing easier.
  static bool IsCommandIdEnabledHelper(
      int command_id,
      omnibox::ChromeAimToolsAndModels aim_tool_mode,
      const std::vector<contextual_search::FileInfo>& file_infos,
      int max_num_files,
      OmniboxPopupState page_type);

  void BuildMenu();
  // Adds a IDC_* style command to the menu with a string16.
  void AddItem(int id, const std::u16string str);
  // Adds a IDC_* style command to the menu with a localized string and icon.
  void AddItemWithStringIdAndIcon(int id,
                                  int localization_id,
                                  const ui::ImageModel& icon);
  // Adds a IDC_* style command to the menu with a string16 and icon.
  void AddItemWithIcon(int command_id,
                       const std::u16string& label,
                       const ui::ImageModel& icon);
  // Adds a separator to the menu.
  void AddSeparator();
  // Adds recent tabs as items to the menu.
  void AddRecentTabItems();
  // Adds the static items with icons.
  void AddStaticItems();
  // Adds a title with a localized string to the menu.
  void AddTitleWithStringId(int localization_id);
  // Gets the most recent tabs.
  std::vector<OmniboxContextMenuController::TabInfo> GetRecentTabs();
  // Adds the tabs favicon to the menu.
  void AddTabFavicon(int command_id,
                     const GURL& url,
                     const std::u16string& label);
  // Callback for when the tab favicon is available.
  void OnFaviconDataAvailable(
      int command_id,
      const favicon_base::FaviconImageResult& image_result);

  void UpdateSearchboxContextToolMode(searchbox::mojom::ToolMode tool_mode);

  raw_ptr<OmniboxController> GetOmniboxController() const;
  raw_ptr<OmniboxEditModel> GetEditModel();
  raw_ptr<OmniboxPopupUI> GetOmniboxPopupUI() const;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  base::WeakPtr<OmniboxPopupFileSelector> file_selector_;
  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<OmniboxEditModel> edit_model_;

  // Needed for using FaviconService.
  base::CancelableTaskTracker cancelable_task_tracker_;
  raw_ptr<FaviconService> favicon_service_;
  int next_command_id_ = 0;

  base::WeakPtrFactory<OmniboxContextMenuController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_
