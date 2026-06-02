// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_

#include <map>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/contextual_search/input_state_model.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "ui/base/models/image_model.h"
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
class OmniboxContextMenuControllerPecBrowserTest;
class OmniboxContextMenuControllerPecBrowserTestWithFlagsDisabled;

namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace ui {
class ImageModel;
}  // namespace ui

namespace omnibox {
enum ToolMode : int;
}  // namespace omnibox

namespace content {
class WebContents;
}  // namespace content

namespace contextual_search {
struct FileInfo;
}  // namespace contextual_search

class OmniboxContextMenuController;

// Child class to override custom font in submenu.
class TabSimpleMenuModel : public ui::SimpleMenuModel {
 public:
  explicit TabSimpleMenuModel(OmniboxContextMenuController* controller);

  const gfx::FontList* GetLabelFontListAt(size_t index) const override;

 private:
  raw_ptr<OmniboxContextMenuController> controller_;
};

enum class OmniboxPopupState;

// OmniboxContextMenuController creates and manages state for the context menu
// shown for the omnibox.
class OmniboxContextMenuController : public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDeepResearchIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFirstTabMenuItemIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kImageUploadMenuItemIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFileUploadMenuItemIdForTesting);
  explicit OmniboxContextMenuController(OmniboxPopupFileSelector* file_selector,
                                        content::WebContents* web_contents);
  struct TabInfo {
    int tab_id;
    std::u16string title;
    GURL url;
    base::TimeTicks last_active;
    bool is_active_tab = false;
    bool is_checked = false;
  };

  OmniboxContextMenuController(const OmniboxContextMenuController&) = delete;
  OmniboxContextMenuController& operator=(const OmniboxContextMenuController&) =
      delete;

  ~OmniboxContextMenuController() override;

  ui::SimpleMenuModel* menu_model() { return menu_model_.get(); }
  ui::SimpleMenuModel* shared_tabs_menu_model() {
    return shared_tabs_menu_model_.get();
  }

  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdEnabledHelper(
      int command_id,
      omnibox::ToolMode aim_tool_mode,
      const std::vector<contextual_search::FileInfo>& file_infos,
      int max_num_files,
      OmniboxPopupState page_type) const;
  bool IsCommandIdVisible(int command_id) const override;
  void AddTabContext(const TabInfo& tab_info);
  static void UpdateSearchboxContext(
      content::WebContents* web_contents,
      std::optional<TabInfo> tab_info,
      std::optional<omnibox::ToolMode> tool_mode,
      std::vector<searchbox::mojom::SearchContextAttachmentPtr> attachments =
          {});

  static void RecordContextMenuItemSelection(const std::string& prefix,
                                             omnibox::ContextType context_type);

  static OmniboxController* GetOmniboxController(
      content::WebContents* web_contents);
  static OmniboxPopupUI* GetOmniboxPopupUI(content::WebContents* web_contents);
  int GetMaxTabSuggestions() const;

 private:
  friend class TabSimpleMenuModel;
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerPecBrowserTest,
                           ModelPickerCheckmark);
  FRIEND_TEST_ALL_PREFIXES(
      OmniboxContextMenuControllerPecBrowserTestWithFlagsDisabled,
      VerifyModelPickerCheckmark_FlagOff);

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
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerTest,
                           GetMaxTabSuggestions_UsesServerLimit);
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerTest,
                           GetIconForInputType_Drive);
  FRIEND_TEST_ALL_PREFIXES(OmniboxContextMenuControllerTest,
                           ExecuteCommand_DriveInputType);

  // Keeps track of various bits of info that are necessary to dynamically
  // render the contents of the context menu, based on the InputState received
  // from the AIM eligibility service.
  struct MenuItemInfo {
    bool enabled = false;
    std::u16string menu_label;
    ui::ImageModel menu_icon;
  };

  // Initializes the various data structures needed to dynamically render the
  // context menu.
  void InitializeMenuItemInfo();
  // Constructs the context menu UI using the imperative SimpleMenuModel API.
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
  // Adds the contextual input items to the menu.
  void AddContextualInputItems();
  // Adds the tool items to the menu.
  void AddToolItems();
  // Adds the model picker items to the menu.
  void AddModelPickerItems();
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
  void OnGetInputState(const std::optional<omnibox::InputState>& input_state);

  void UpdateSearchboxContextToolMode(omnibox::ToolMode tool_mode);

  bool IsContentSharingEnabled() const;

  omnibox::ContextType CommandIdToEnum(int command_id) const;

  void RecordContextMenuItemSelection(const std::string& prefix,
                                      int command_id);

  /* Helpers for InputType input_state fields. */
  const omnibox::InputTypeConfig* GetInputTypeConfig(
      omnibox::InputType input_type) const;
  bool IsInputTypeEnabled(omnibox::InputType input_type) const;
  std::u16string GetMenuLabelForInputType(omnibox::InputType input_type) const;
  ui::ImageModel GetIconForInputType(omnibox::InputType input_type) const;

  /* Helpers for ToolMode input_state fields. */
  const omnibox::ToolConfig* GetToolConfig(omnibox::ToolMode tool) const;
  std::optional<omnibox::SectionConfig> GetToolSectionConfig() const;
  bool IsToolEnabled(omnibox::ToolMode tool) const;
  std::u16string GetMenuLabelForTool(omnibox::ToolMode tool) const;
  ui::ImageModel GetIconForTool(omnibox::ToolMode tool) const;

  /* Helpers for ModelMode input_state fields. */
  const omnibox::ModelConfig* GetModelConfig(omnibox::ModelMode model) const;
  std::optional<omnibox::SectionConfig> GetModelSectionConfig() const;
  bool IsModelEnabled(omnibox::ModelMode model) const;
  std::u16string GetMenuLabelForModel(omnibox::ModelMode model) const;
  ui::ImageModel GetIconForModel(omnibox::ModelMode model) const;

  raw_ptr<OmniboxController> GetOmniboxController() const;
  raw_ptr<OmniboxEditModel> GetEditModel();
  raw_ptr<OmniboxPopupUI> GetOmniboxPopupUI() const;

  std::unique_ptr<TabSimpleMenuModel> menu_model_;
  std::unique_ptr<TabSimpleMenuModel> shared_tabs_menu_model_;
  base::WeakPtr<OmniboxPopupFileSelector> file_selector_;
  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<OmniboxEditModel> edit_model_;

  // Needed for using FaviconService.
  base::CancelableTaskTracker cancelable_task_tracker_;
  raw_ptr<FaviconService> favicon_service_;
  int next_command_id_ = 0;
  int min_tools_and_models_command_id_ = 0;

  omnibox::InputState input_state_;

  std::map<omnibox::InputType, MenuItemInfo> input_type_info_;
  std::map<int, omnibox::InputType> input_type_for_command_id_;

  std::map<omnibox::ToolMode, MenuItemInfo> tool_info_;
  std::map<int, omnibox::ToolMode> tool_for_command_id_;

  std::map<omnibox::ModelMode, MenuItemInfo> model_info_;
  std::map<int, omnibox::ModelMode> model_for_command_id_;

  base::WeakPtrFactory<OmniboxContextMenuController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTEXT_MENU_CONTROLLER_H_
