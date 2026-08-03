// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/features.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/model_config.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/section_config.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image.h"

namespace {
constexpr int kMinOmniboxContextMenuRecentTabsCommandId = 33000;
// Default limit for the maximum number of files if the input state is
// uninitialized.
constexpr int kDefaultMaxNumFiles = 10;

std::u16string GetSmartTabSharingMegaplusMenuLabel() {
  switch (contextual_tasks::kSmartTabSharingMegaplusStringOption.Get()) {
    case contextual_tasks::SmartTabSharingMegaplusStringOption::kMegaplusV1:
      return l10n_util::GetStringUTF16(
          IDS_STS_MEGAPLUS_SHARE_RELEVANT_OPEN_TABS);
    case contextual_tasks::SmartTabSharingMegaplusStringOption::kMegaplusV2:
      return l10n_util::GetStringUTF16(
          IDS_STS_MEGAPLUS_SHARE_RELEVANT_OPEN_TABS_V2);
    case contextual_tasks::SmartTabSharingMegaplusStringOption::kMegaplusV3:
      return l10n_util::GetStringUTF16(
          IDS_STS_MEGAPLUS_SHARE_RELEVANT_OPEN_TABS_V3);
    default:
      return l10n_util::GetStringUTF16(
          IDS_STS_MEGAPLUS_SHARE_RELEVANT_OPEN_TABS);
  }
}

}  // namespace

TabSimpleMenuModel::TabSimpleMenuModel(OmniboxContextMenuController* controller)
    : ui::SimpleMenuModel(controller), controller_(controller) {}

const gfx::FontList* TabSimpleMenuModel::GetLabelFontListAt(
    size_t index) const {
  if (GetTypeAt(index) == ui::MenuModel::TYPE_TITLE) {
    return ui::SimpleMenuModel::GetLabelFontListAt(index);
  }

  // Explicitly force the standard font for all interactive items.
  return &ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::BaseFont);
}

std::optional<ui::ColorId> TabSimpleMenuModel::GetForegroundColorId(
    size_t index) {
  int command_id = GetCommandIdAt(index);
  if (command_id == IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU) {
    if (!controller_->IsTabContextEnabled()) {
      return ui::kColorMenuItemForegroundDisabled;
    }
  }
  return ui::SimpleMenuModel::GetForegroundColorId(index);
}

std::optional<ui::ColorId> TabSimpleMenuModel::GetSelectedBackgroundColorId(
    size_t index) {
  int command_id = GetCommandIdAt(index);
  if (command_id == IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU) {
    if (!controller_->IsTabContextEnabled()) {
      return ui::kColorMenuItemBackgroundSelected;
    }
  }
  return ui::SimpleMenuModel::GetSelectedBackgroundColorId(index);
}

namespace {

bool IsStaticOmniboxCommandId(int command_id) {
  switch (command_id) {
    case IDC_OMNIBOX_CONTEXT_ADD_IMAGE:
    case IDC_OMNIBOX_CONTEXT_ADD_FILE:
    case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
    case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
    case IDC_OMNIBOX_CONTEXT_CANVAS:
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO:
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING:
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR:
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_PRO_NO_GEN_UI:
    case IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU:
      return true;
    default:
      return false;
  }
}

bool IsValidTab(GURL url) {
  // Skip tabs that are still loading, and skip webui.
  return url.is_valid() && !url.is_empty() &&
         !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeUIUntrustedScheme) &&
         !url.IsAboutBlank();
}

bool IsThinkingModel(omnibox::ModelMode model) {
  return model == omnibox::ModelMode::MODEL_MODE_GEMINI_PRO ||
         model == omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OmniboxContextMenuController,
                                      kDeepResearchIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OmniboxContextMenuController,
                                      kFirstTabMenuItemIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OmniboxContextMenuController,
                                      kImageUploadMenuItemIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OmniboxContextMenuController,
                                      kFileUploadMenuItemIdForTesting);

OmniboxContextMenuController::OmniboxContextMenuController(
    OmniboxPopupFileSelector* file_selector,
    content::WebContents* web_contents)
    : file_selector_(file_selector->GetWeakPtr()),
      web_contents_(web_contents->GetWeakPtr()) {
  menu_model_ = std::make_unique<TabSimpleMenuModel>(this);
  next_command_id_ = kMinOmniboxContextMenuRecentTabsCommandId;
  auto* composebox_handler =
      GetOmniboxPopupUI() ? GetOmniboxPopupUI()->composebox_handler() : nullptr;
  if (composebox_handler &&
      base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    // Pre-populate `input_state_` synchronously from the cached model state
    // so dynamic items like recent tabs are available during initial menu
    // build. Otherwise, menu will not have tabs.
    if (composebox_handler->input_state_model()) {
      input_state_ = composebox_handler->input_state_model()->GetInputState();
    }
    composebox_handler->GetInputState(
        base::BindOnce(&OmniboxContextMenuController::OnGetInputState,
                       weak_ptr_factory_.GetWeakPtr()));
    InitializeMenuItemInfo();
  }
  // Set remaining command ID start point. If max tabs
  // is known, reserve command ID's now. Otherwise, tab
  // command ID's will be dynamically added later for tabs.
  std::optional<size_t> max_suggestions = GetMaxTabSuggestions();
  min_tools_and_models_command_id_ =
      kMinOmniboxContextMenuRecentTabsCommandId +
      static_cast<int>(max_suggestions.value_or(0));
  BuildMenu();
}

OmniboxContextMenuController::~OmniboxContextMenuController() = default;

void OmniboxContextMenuController::InitializeMenuItemInfo() {
  for (omnibox::InputType input_type : input_state_.allowed_input_types) {
    input_type_info_.insert(
        {input_type,
         {/*enabled=*/IsInputTypeEnabled(input_type),
          /*menu_label=*/GetMenuLabelForInputType(input_type),
          /*menu_icon=*/GetIconForInputType(input_type)}});
  }

  for (omnibox::ToolMode tool : input_state_.allowed_tools) {
    tool_info_.insert({tool,
                       {/*enabled=*/IsToolEnabled(tool),
                        /*menu_label=*/GetMenuLabelForTool(tool),
                        /*menu_icon=*/GetIconForTool(tool)}});
  }

  for (omnibox::ModelMode model : input_state_.allowed_models) {
    model_info_.insert({model,
                        {/*enabled=*/IsModelEnabled(model),
                         /*menu_label=*/GetMenuLabelForModel(model),
                         /*menu_icon=*/GetIconForModel(model)}});
  }
}

void OmniboxContextMenuController::BuildMenu() {
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    auto is_browser_tab =
        [](const std::pair<omnibox::InputType, MenuItemInfo> p) {
          return p.first == omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
        };

    auto browser_tab_it = std::find_if(input_type_info_.begin(),
                                       input_type_info_.end(), is_browser_tab);
    if (browser_tab_it != input_type_info_.end()) {
      AddRecentTabItems();
    }
    auto non_browser_tab_it = std::find_if_not(
        input_type_info_.begin(), input_type_info_.end(), is_browser_tab);
    if (non_browser_tab_it != input_type_info_.end()) {
      AddSeparator();
      AddContextualInputItems();
    }
    if (!tool_info_.empty()) {
      AddSeparator();
      AddToolItems();
    }
    if (!model_info_.empty()) {
      AddSeparator();
      AddModelPickerItems();
    }
  } else {
    AddRecentTabItems();
    AddContextualInputItems();
    AddToolItems();
  }
}

void OmniboxContextMenuController::AddItem(int id, const std::u16string str) {
  menu_model_->AddItem(id, str);
}

void OmniboxContextMenuController::AddItemWithStringIdAndIcon(
    int id,
    int localization_id,
    const ui::ImageModel& icon) {
  menu_model_->AddItemWithStringIdAndIcon(id, localization_id, icon);
}

void OmniboxContextMenuController::AddItemWithIcon(int command_id,
                                                   const std::u16string& label,
                                                   const ui::ImageModel& icon) {
  menu_model_->AddItemWithIcon(command_id, label, icon);
}

void OmniboxContextMenuController::AddSeparator() {
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
}

void OmniboxContextMenuController::AddRecentTabItems() {
  if (!IsContentSharingEnabled()) {
    return;
  }

  std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();
  if (tabs.empty()) {
    return;
  }

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  Profile* profile = browser_window_interface->GetProfile();
  const bool is_smart_tab_sharing_enabled =
      profile && contextual_tasks::ContextualTasksContextService::
                     GetIsSmartTabSharingEnabled(profile);

  auto* handler = GetSearchboxHandler();
  const bool is_smart_tab_sharing_active =
      handler && handler->IsSmartTabSharingActive();

  const bool include_tabs_submenu =
      base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox) &&
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox);

  // If Smart Tab Sharing is active, display the toggle directly in the main
  // menu, and do not show the manual tab sharing submenu.
  if (include_tabs_submenu && is_smart_tab_sharing_enabled &&
      is_smart_tab_sharing_active) {
    size_t index = menu_model_->GetItemCount();
    menu_model_->AddItem(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING,
                         GetSmartTabSharingMegaplusMenuLabel());
    menu_model_->SetIconForCommandId(
        IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING,
        ui::ImageModel::FromVectorIcon(kTabOldIcon, ui::kColorMenuIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
    menu_model_->SetMinorIcon(
        index,
        ui::ImageModel::FromVectorIcon(
            features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
            ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize));
    menu_model_->SetMinorIconOnRight(
        ui::MenuModel::MinorIconOnRightPasskey(index),
        /*minor_icon_on_right=*/true);
    return;
  }

  ui::SimpleMenuModel* target_menu_model;
  size_t first_tab_index;

  // `target_menu_model` will either become the submenu, or a section in main
  // menu.
  if (include_tabs_submenu) {
    // Add shared tabs submenu to main menu and make submenu the tabs
    // `target_menu_model`.
    shared_tabs_menu_model_ = std::make_unique<TabSimpleMenuModel>(this);
    target_menu_model = shared_tabs_menu_model_.get();
    first_tab_index = 0;

    // If Smart Tab Sharing is enabled but NOT active, put the toggle at the
    // top of the submenu.
    if (is_smart_tab_sharing_enabled) {
      shared_tabs_menu_model_->AddItem(IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING,
                                       GetSmartTabSharingMegaplusMenuLabel());
      shared_tabs_menu_model_->SetIconForCommandId(
          IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING,
          ui::ImageModel::FromVectorIcon(
              kScreensaverAutoIcon, ui::kColorMenuIcon,
              ui::SimpleMenuModel::kDefaultIconSize));
      shared_tabs_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      first_tab_index = shared_tabs_menu_model_->GetItemCount();
    }
  } else {
    // Add default title for 'Recent tabs' when there is no shared tabs menu.
    if (omnibox::kShowContextMenuHeaders.Get()) {
      AddTitleWithStringId(IDS_NTP_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE);
    }
    // Part of screen where the tabs will be placed in.
    target_menu_model = menu_model_.get();
    // The first tab will be appended at the current item count index.
    first_tab_index = menu_model_->GetItemCount();
  }

  // Add tabs to the target destination in UI.
  bool has_active_tab = std::ranges::any_of(
      tabs, [](const TabInfo& tab) { return tab.is_active_tab; });

  size_t tab_index = 0;
  for (const auto& tab : tabs) {
    target_menu_model->AddItemWithIcon(next_command_id_, tab.title,
                                       favicon::GetDefaultFaviconModel());
    if (include_tabs_submenu) {
      if (tab.is_active_tab) {
        target_menu_model->SetMinorText(
            target_menu_model->GetItemCount() - 1,
            l10n_util::GetStringUTF16(IDS_COMPOSE_CURRENT_TAB));
      } else if (!has_active_tab && tab_index == 0) {
        target_menu_model->SetMinorText(
            target_menu_model->GetItemCount() - 1,
            l10n_util::GetStringUTF16(IDS_NTP_COMPOSEBOX_RECENT_TAB_SUFFIX));
      }
    }
    AddTabFavicon(next_command_id_, tab.url, tab.title);
    input_type_for_command_id_[next_command_id_] =
        omnibox::InputType::INPUT_TYPE_BROWSER_TAB;

    // If tab has been staged for uploading,
    // add a check mark icon.
    if (tab.is_checked &&
        base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
        base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox)) {
      size_t index = target_menu_model->GetItemCount() - 1;
      auto check_icon = ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
          ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
      target_menu_model->SetMinorIcon(index, check_icon);
      // Set checkmark icon on the right.
      target_menu_model->SetMinorIconOnRight(
          ui::MenuModel::MinorIconOnRightPasskey(index), true);
    }

    next_command_id_ += 1;
    tab_index++;
  }

  // Add submenu name and icon.
  if (include_tabs_submenu) {
    int checked_count = std::ranges::count_if(tabs, &TabInfo::is_checked);
    std::u16string label;
    if (checked_count > 0) {
      label = l10n_util::GetPluralStringFUTF16(IDS_COMPOSE_SHARING_TABS,
                                               checked_count);
    } else {
      label = l10n_util::GetStringUTF16(IDS_COMPOSE_ADD_TABS);
    }
    menu_model_->AddSubMenuWithIcon(
        IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU, label,
        shared_tabs_menu_model_.get(),
        ui::ImageModel::FromVectorIcon(kTabOldIcon,
                                       IsTabContextEnabled()
                                           ? ui::kColorMenuIcon
                                           : ui::kColorMenuIconDisabled,
                                       ui::SimpleMenuModel::kDefaultIconSize));
  }

  min_tools_and_models_command_id_ =
      std::max(min_tools_and_models_command_id_, next_command_id_);

  // ID for testing tab section.
  target_menu_model->SetElementIdentifierAt(first_tab_index,
                                            kFirstTabMenuItemIdForTesting);

  if (!base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    AddSeparator();
  }
}

void OmniboxContextMenuController::AddContextualInputItems() {
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    next_command_id_ = min_tools_and_models_command_id_;
    for (const auto input_type : input_state_.allowed_input_types) {
      // BROWSER_TAB input type is handled by `AddRecentTabItems()`.
      if (input_type == omnibox::InputType::INPUT_TYPE_BROWSER_TAB) {
        continue;
      }
      auto& menu_item_info = input_type_info_[input_type];
      AddItemWithIcon(next_command_id_, menu_item_info.menu_label,
                      menu_item_info.menu_icon);
      if (input_type == omnibox::InputType::INPUT_TYPE_LENS_IMAGE) {
        menu_model_->SetElementIdentifierAt(menu_model_->GetItemCount() - 1,
                                            kImageUploadMenuItemIdForTesting);
      } else if (input_type == omnibox::InputType::INPUT_TYPE_LENS_FILE) {
        menu_model_->SetElementIdentifierAt(menu_model_->GetItemCount() - 1,
                                            kFileUploadMenuItemIdForTesting);
      }
      input_type_for_command_id_[next_command_id_] = input_type;
      next_command_id_++;
    }
    min_tools_and_models_command_id_ = next_command_id_;
  } else {
    auto add_image_icon = ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? kAddPhotoAlternateIcon
                                          : kAddPhotoAlternateOldIcon,
        ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
                               IDS_NTP_COMPOSE_ADD_IMAGE, add_image_icon);

    auto add_file_icon = ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? kAttachFileIcon
                                          : kAttachFileOldIcon,
        ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_ADD_FILE,
                               IDS_NTP_COMPOSE_ADD_FILE, add_file_icon);
  }
}

void OmniboxContextMenuController::AddToolItems() {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  Profile* profile = browser_window_interface->GetProfile();

  bool use_pec_api = base::FeatureList::IsEnabled(omnibox::kAimUsePecApi);
  if (!use_pec_api && (omnibox::IsDeepSearchEnabled(profile) ||
                       omnibox::IsCreateImagesEnabled(profile))) {
    AddSeparator();
  }

  auto create_images_icon = ui::ImageModel::FromResourceId(
      IDR_OMNIBOX_POPUP_IMAGES_CREATE_IMAGES_PNG);
  auto deep_search_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kTravelExploreIcon
                                        : kTravelExploreOldIcon,
      ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);

  if (use_pec_api) {
    auto tool_section_config = GetToolSectionConfig();
    if (omnibox::kShowContextMenuHeaders.Get() && tool_section_config &&
        !tool_section_config->header().empty()) {
      menu_model_->AddTitle(base::UTF8ToUTF16(tool_section_config->header()));
    }

    next_command_id_ = min_tools_and_models_command_id_;
    for (const auto tool : input_state_.allowed_tools) {
      auto& menu_item_info = tool_info_[tool];
      AddItemWithIcon(next_command_id_, menu_item_info.menu_label,
                      menu_item_info.menu_icon);
      if (tool == omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH) {
        menu_model_->SetElementIdentifierAt(menu_model_->GetItemCount() - 1,
                                            kDeepResearchIdForTesting);
      }
      tool_for_command_id_[next_command_id_] = tool;
      next_command_id_++;
    }
    min_tools_and_models_command_id_ = next_command_id_;
  } else {
    AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
                               IDS_NTP_COMPOSE_CREATE_IMAGES,
                               create_images_icon);

    AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
                               IDS_NTP_COMPOSE_DEEP_SEARCH, deep_search_icon);
    menu_model_->SetElementIdentifierAt(menu_model_->GetItemCount() - 1,
                                        kDeepResearchIdForTesting);
  }
}

void OmniboxContextMenuController::AddModelPickerItems() {
  bool is_aim_popup_open =
      GetOmniboxController() &&
      GetOmniboxController()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim;

  auto model_section_config = GetModelSectionConfig();
  if (omnibox::kShowContextMenuHeaders.Get() && model_section_config &&
      !model_section_config->header().empty()) {
    menu_model_->AddTitle(base::UTF8ToUTF16(model_section_config->header()));
  }

  auto thinking_model_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kTimerIcon : kTimerOldIcon,
      ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);

  auto check_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
      ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);

  const bool show_rhs_checkmark =
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
      base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox);

  next_command_id_ = min_tools_and_models_command_id_;
  for (const auto model : input_state_.allowed_models) {
    auto& menu_item_info = model_info_[model];
    const auto& menu_icon =
        IsThinkingModel(model) ? thinking_model_icon : menu_item_info.menu_icon;

    // If relevant flag is enabled, the checkmark is shown on the right.
    // Otherwise, it is shown on the left.
    const auto& lhs_icon = (!show_rhs_checkmark && is_aim_popup_open &&
                            input_state_.active_model == model)
                               ? check_icon
                               : menu_icon;

    AddItemWithIcon(next_command_id_, menu_item_info.menu_label, lhs_icon);
    if (show_rhs_checkmark && is_aim_popup_open &&
        input_state_.active_model == model) {
      size_t index = menu_model_->GetItemCount() - 1;
      menu_model_->SetMinorIcon(index, check_icon);
      menu_model_->SetMinorIconOnRight(
          ui::MenuModel::MinorIconOnRightPasskey(index), true);
    }
    model_for_command_id_[next_command_id_] = model;
    next_command_id_++;
  }
  min_tools_and_models_command_id_ = next_command_id_;
}

std::vector<OmniboxContextMenuController::TabInfo>
OmniboxContextMenuController::GetRecentTabs() const {
  std::vector<OmniboxContextMenuController::TabInfo> tabs;

  std::vector<contextual_search::FileInfo> uploaded_file_infos;
  auto* session_handle =
      GetOmniboxPopupUI()
          ? GetOmniboxPopupUI()->GetOrCreateContextualSessionHandle()
          : nullptr;
  if (session_handle) {
    uploaded_file_infos = session_handle->GetUploadedContextFileInfos();
  }

  // Iterate through the tab strip model.
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  if (!browser_window_interface) {
    return tabs;
  }
  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  if (!tab_strip_model) {
    return tabs;
  }
  for (tabs::TabInterface* tab : *tab_strip_model) {
    content::WebContents* web_contents = tab->GetContents();
    const auto& last_committed_url = web_contents->GetLastCommittedURL();
    if (!IsValidTab(last_committed_url)) {
      continue;
    }

    OmniboxContextMenuController::TabInfo tab_data;
    tab_data.tab_id = tab->GetHandle().raw_value();
    tab_data.title = TabUIHelper::From(tab)->GetTitle();
    tab_data.url = last_committed_url;
    tab_data.is_active_tab = (tab == tab_strip_model->GetActiveTab());

    bool is_checked = false;
    if (auto* omnibox_popup_ui = GetOmniboxPopupUI()) {
      auto* composebox_handler = omnibox_popup_ui->composebox_handler();
      if (composebox_handler) {
        is_checked = std::ranges::any_of(
            composebox_handler->selected_tabs,
            [&](const auto& pair) { return tab_data.tab_id == pair.second; });
      }
      auto* omnibox_handler = omnibox_popup_ui->omnibox_handler();
      if (!is_checked && omnibox_handler) {
        is_checked = std::ranges::any_of(
            omnibox_handler->selected_tabs,
            [&](const auto& pair) { return tab_data.tab_id == pair.second; });
      }
    }
    tab_data.is_checked = is_checked;

    tab_data.last_active =
        std::max(web_contents->GetLastActiveTimeTicks(),
                 web_contents->GetLastInteractionTimeTicks());
    tabs.push_back(tab_data);
  }

  // Sort tabs by most recently active, up to `max_suggestions`
  // number of tabs. Checked (selected) tabs are first; ties broken by most
  // recent.
  std::optional<size_t> max_suggestions = GetMaxTabSuggestions();
  // Max tab suggestions allowed is infinite if nullopt is returned,
  // so use current number of tabs; otherwise, limit sorted tabs to
  // the number of max tabs.
  size_t max_tab_suggestions = max_suggestions.value_or(tabs.size());
  max_tab_suggestions = std::min(tabs.size(), max_tab_suggestions);
  std::partial_sort(tabs.begin(), tabs.begin() + max_tab_suggestions,
                    tabs.end(),
                    [](const OmniboxContextMenuController::TabInfo& a,
                       const OmniboxContextMenuController::TabInfo& b) {
                      if (a.is_checked != b.is_checked) {
                        return a.is_checked > b.is_checked;
                      }
                      return a.last_active > b.last_active;
                    });
  tabs.resize(max_tab_suggestions);
  cached_recent_tabs_ = tabs;
  return tabs;
}

bool OmniboxContextMenuController::IsTabContextEnabled() const {
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    auto it = input_type_info_.find(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
    return it != input_type_info_.end() && it->second.enabled;
  }

  auto* omnibox_controller = GetOmniboxController();
  if (!omnibox_controller) {
    return false;
  }
  auto* omnibox_popup_ui = GetOmniboxPopupUI();
  if (!omnibox_popup_ui || !omnibox_popup_ui->composebox_handler() ||
      !omnibox_popup_ui->composebox_handler()->input_state_model()) {
    return false;
  }

  auto* model = omnibox_popup_ui->composebox_handler()->input_state_model();
  const auto& disabled_types = model->GetInputState().disabled_input_types;
  if (std::ranges::contains(disabled_types,
                            omnibox::InputType::INPUT_TYPE_BROWSER_TAB)) {
    return false;
  }

  return true;
}

void OmniboxContextMenuController::AddTabFavicon(int command_id,
                                                 const GURL& url,
                                                 const std::u16string& label) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  Profile* profile = browser_window_interface->GetProfile();
  if (!profile) {
    return;
  }
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service) {
    return;
  }

  favicon_service->GetFaviconImageForPageURL(
      url,
      base::BindOnce(static_cast<void (OmniboxContextMenuController::*)(
                         int, const favicon_base::FaviconImageResult&)>(
                         &OmniboxContextMenuController::OnFaviconDataAvailable),
                     weak_ptr_factory_.GetWeakPtr(), command_id),
      &cancelable_task_tracker_);
}

void OmniboxContextMenuController::OnFaviconDataAvailable(
    int command_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (image_result.image.IsEmpty()) {
    // Default icon has already been set.
    return;
  }

  // Look through particular menu model (either tab submenu, or recent
  // tabs section).
  ui::SimpleMenuModel* target_menu_model = menu_model_.get();
  std::optional<size_t> index_in_menu =
      target_menu_model->GetIndexOfCommandId(command_id);
  if (!index_in_menu && shared_tabs_menu_model_) {  // Target submenu instead.
    target_menu_model = shared_tabs_menu_model_.get();
    index_in_menu = target_menu_model->GetIndexOfCommandId(command_id);
  }
  DCHECK(index_in_menu.has_value());
  target_menu_model->SetIcon(index_in_menu.value(),
                             ui::ImageModel::FromImage(image_result.image));
  // Update the screen after updating the icon.
  if (target_menu_model->menu_model_delegate()) {
    target_menu_model->menu_model_delegate()->OnIconChanged(command_id);
  }
}

void OmniboxContextMenuController::OnGetInputState(
    const std::optional<omnibox::InputState>& input_state) {
  if (input_state) {
    input_state_ = *input_state;
  }
}

void OmniboxContextMenuController::AddTitleWithStringId(int localization_id) {
  menu_model_->AddTitleWithStringId(localization_id);
}

void OmniboxContextMenuController::AddTabContext(const TabInfo& tab_info) {
  UpdateSearchboxContext(web_contents_.get(), /*tab_info=*/tab_info,
                         /*tool_mode=*/std::nullopt);
  OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
}

// static
void OmniboxContextMenuController::UpdateSearchboxContext(
    content::WebContents* web_contents,
    std::optional<TabInfo> tab_info,
    std::optional<omnibox::ToolMode> tool_mode,
    std::vector<searchbox::mojom::SearchContextAttachmentPtr> attachments) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser_window_interface) {
    return;
  }
  SearchboxContextData* searchbox_context_data =
      browser_window_interface->GetFeatures().searchbox_context_data();
  if (!searchbox_context_data) {
    return;
  }
  auto context = searchbox_context_data->TakePendingContext();
  if (!context) {
    context = std::make_unique<SearchboxContextData::Context>();
  }

  if (tab_info) {
    auto tab_attachment = searchbox::mojom::TabAttachment::New();
    tab_attachment->tab_id = tab_info->tab_id;
    tab_attachment->title = base::UTF16ToUTF8(tab_info->title);
    tab_attachment->url = tab_info->url;
    context->file_infos.push_back(
        searchbox::mojom::SearchContextAttachment::NewTabAttachment(
            std::move(tab_attachment)));
  }

  for (auto& attachment : attachments) {
    context->file_infos.push_back(std::move(attachment));
  }

  if (tool_mode) {
    context->mode = *tool_mode;
  }

  auto* omnibox_controller = GetOmniboxController(web_contents);

  if (omnibox_controller &&
      omnibox_controller->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim) {
    auto* omnibox_popup_ui = GetOmniboxPopupUI(web_contents);
    if (omnibox_popup_ui && omnibox_popup_ui->popup_aim_handler()) {
      omnibox_popup_ui->popup_aim_handler()->AddContext(std::move(context));
    }
  } else {
    searchbox_context_data->SetPendingContext(std::move(context));
  }
}

void OmniboxContextMenuController::HandleDriveUploadResponse(
    bool was_ai_mode_open,
    base::WeakPtr<content::WebContents> web_contents,
    searchbox::mojom::DriveUploadResponsePtr response) {
  if (!response || !web_contents) {
    return;
  }

  std::vector<searchbox::mojom::SearchContextAttachmentPtr> file_attachments;
  for (const auto& file : response->files) {
    auto file_attachment = searchbox::mojom::FileAttachment::New();
    file_attachment->uuid = file->token;
    file_attachment->name = file->file_name;
    file_attachment->mime_type = file->mime_type;
    file_attachment->image_data_url = std::nullopt;
    file_attachment->icon_url = file->icon_url;
    if (file->thumbnail_url.has_value()) {
      file_attachment->thumbnail_url = GURL(file->thumbnail_url.value());
    }

    file_attachments.push_back(
        searchbox::mojom::SearchContextAttachment::NewFileAttachment(
            std::move(file_attachment)));
  }

  if (response->error.has_value()) {
    auto file_attachment = searchbox::mojom::FileAttachment::New();
    file_attachment->uuid = base::UnguessableToken::Create();
    file_attachment->name = "";
    file_attachment->mime_type = "";

    contextual_search::ContextUploadErrorType error_type =
        contextual_search::ContextUploadErrorType::kUnknown;
    switch (response->error.value()) {
      case searchbox::mojom::DriveUploadError::kMaxFilesExceeded:
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingMaxFilesExceededError;
        break;
      case searchbox::mojom::DriveUploadError::kSizeLimitExceeded:
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingFileTooLargeError;
        break;
    }
    file_attachment->error_type = error_type;
    file_attachments.push_back(
        searchbox::mojom::SearchContextAttachment::NewFileAttachment(
            std::move(file_attachment)));
  }

  // Only update the search context and open AI mode if files or errors were
  // received. Ignore cancellations to preserve the current search/query state.
  bool has_files_or_errors = !file_attachments.empty();
  if (has_files_or_errors) {
    OmniboxContextMenuController::UpdateSearchboxContext(
        web_contents.get(), /*tab_info=*/std::nullopt,
        /*tool_mode=*/std::nullopt, std::move(file_attachments));
  }

  auto* omnibox_controller =
      OmniboxContextMenuController::GetOmniboxController(web_contents.get());
  if (omnibox_controller && omnibox_controller->edit_model()) {
    if (was_ai_mode_open || has_files_or_errors) {
      omnibox_controller->edit_model()->OpenAiMode(
          OmniboxEditModel::AimActivation::kContextMenu);
    }
  }
}

bool OmniboxContextMenuController::IsContentSharingEnabled() const {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  if (!browser_window_interface) {
    return false;
  }
  Profile* profile = browser_window_interface->GetProfile();
  if (!profile) {
    return false;
  }
  auto* session_handle =
      GetOmniboxPopupUI()
          ? GetOmniboxPopupUI()->GetOrCreateContextualSessionHandle()
          : nullptr;
  return omnibox::IsContentSharingEnabled(profile, session_handle);
}

std::optional<size_t> OmniboxContextMenuController::GetMaxTabSuggestions()
    const {
  if (auto it = input_state_.max_inputs_by_type.find(
          omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
      it != input_state_.max_inputs_by_type.end()) {
    if (it->second < 0) {
      return std::nullopt;
    }
    return static_cast<size_t>(it->second);
  }
  // If `kContextManagementInComposebox` and `kContextManagementInOmnibox`
  // are enabled, there is no maximum tab limit.
  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
      base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox)) {
    return std::nullopt;
  }
  int limit = omnibox::kContextMenuMaxTabSuggestions.Get();
  if (limit < 0) {
    return std::nullopt;
  }
  return static_cast<size_t>(limit);
}

bool OmniboxContextMenuController::IsTabCommandId(int command_id) const {
  if (command_id < kMinOmniboxContextMenuRecentTabsCommandId) {
    return false;
  }
  if (IsStaticOmniboxCommandId(command_id)) {
    return false;
  }
  if (tool_for_command_id_.find(command_id) != tool_for_command_id_.end() ||
      model_for_command_id_.find(command_id) != model_for_command_id_.end()) {
    return false;
  }
  if (auto it = input_type_for_command_id_.find(command_id);
      it != input_type_for_command_id_.end()) {
    return it->second == omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
  }
  std::optional<size_t> max_suggestions = GetMaxTabSuggestions();
  if (max_suggestions.has_value()) {
    return command_id < kMinOmniboxContextMenuRecentTabsCommandId +
                            static_cast<int>(max_suggestions.value());
  }
  return true;
}

omnibox::ContextType OmniboxContextMenuController::CommandIdToEnum(
    int command_id) const {
  // Translate to generic omnibox type for logging, etc.
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    if (auto it = input_type_for_command_id_.find(command_id);
        it != input_type_for_command_id_.end()) {
      switch (it->second) {
        case omnibox::InputType::INPUT_TYPE_BROWSER_TAB:
          return omnibox::ContextType::kTab;
        case omnibox::InputType::INPUT_TYPE_LENS_IMAGE:
          return omnibox::ContextType::kImage;
        case omnibox::InputType::INPUT_TYPE_LENS_FILE:
          return omnibox::ContextType::kFile;
        case omnibox::InputType::INPUT_TYPE_DRIVE:
          return omnibox::ContextType::kDrive;
        default:
          return omnibox::ContextType::kUnknown;
      }
    }

    if (auto it = tool_for_command_id_.find(command_id);
        it != tool_for_command_id_.end()) {
      switch (it->second) {
        case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
          return omnibox::ContextType::kImageGen;
        case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
          return omnibox::ContextType::kDeepResearch;
        case omnibox::ToolMode::TOOL_MODE_CANVAS:
          return omnibox::ContextType::kCanvas;
        default:
          return omnibox::ContextType::kUnknown;
      }
    }

    if (auto it = model_for_command_id_.find(command_id);
        it != model_for_command_id_.end()) {
      switch (it->second) {
        case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
          return omnibox::ContextType::kAutoModel;
        case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
          return omnibox::ContextType::kRegularModel;
        case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
          return omnibox::ContextType::kThinkingModel;
        case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
          return omnibox::ContextType::kProNoGenUiModel;
        default:
          return omnibox::ContextType::kUnknown;
      }
    }
  }

  switch (command_id) {
    case IDC_OMNIBOX_CONTEXT_ADD_IMAGE:
      return omnibox::ContextType::kImage;
    case IDC_OMNIBOX_CONTEXT_ADD_FILE:
      return omnibox::ContextType::kFile;
    case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
      return omnibox::ContextType::kImageGen;
    case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
      return omnibox::ContextType::kDeepResearch;
    case IDC_OMNIBOX_CONTEXT_CANVAS:
      return omnibox::ContextType::kCanvas;
    default:
      // There is no command id for tabs due to there being multiple
      // tabs that would have the same command id.
      CHECK(IsTabCommandId(command_id));
      return omnibox::ContextType::kTab;
  }
}

void OmniboxContextMenuController::RecordContextMenuItemSelection(
    const std::string& prefix,
    omnibox::ContextType context_type) {
  base::UmaHistogramEnumeration(prefix, context_type);
  std::string action_name =
      base::StrCat({prefix, ".", omnibox::GetContextTypeString(context_type)});

  base::RecordAction(base::UserMetricsAction(action_name.c_str()));
}

void OmniboxContextMenuController::RecordContextMenuItemSelection(
    const std::string& prefix,
    int command_id) {
  RecordContextMenuItemSelection(prefix, CommandIdToEnum(command_id));
}

const omnibox::InputTypeConfig*
OmniboxContextMenuController::GetInputTypeConfig(
    omnibox::InputType input_type) const {
  auto it = std::find_if(input_state_.input_type_configs.begin(),
                         input_state_.input_type_configs.end(),
                         [&](const omnibox::InputTypeConfig config) {
                           return config.input_type() == input_type;
                         });
  return (it != input_state_.input_type_configs.end()) ? &(*it) : nullptr;
}

bool OmniboxContextMenuController::IsInputTypeEnabled(
    omnibox::InputType input_type) const {
  return std::none_of(input_state_.disabled_input_types.begin(),
                      input_state_.disabled_input_types.end(),
                      [&](omnibox::InputType disabled_input_type) {
                        return disabled_input_type == input_type;
                      });
}

std::u16string OmniboxContextMenuController::GetMenuLabelForInputType(
    omnibox::InputType input_type) const {
  auto* input_type_config = GetInputTypeConfig(input_type);
  if (input_type_config && !input_type_config->menu_label().empty()) {
    return base::UTF8ToUTF16(input_type_config->menu_label());
  }

  // If the server didn't provide a menu label, return a fallback value.
  switch (input_type) {
    case omnibox::InputType::INPUT_TYPE_LENS_IMAGE:
      return l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_IMAGE);
    case omnibox::InputType::INPUT_TYPE_LENS_FILE:
      return l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_FILE);
    case omnibox::InputType::INPUT_TYPE_DRIVE:
      return l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_DRIVE);
    default:
      return u"";
  }
}

ui::ImageModel OmniboxContextMenuController::GetIconForInputType(
    omnibox::InputType input_type) const {
  switch (input_type) {
    case omnibox::InputType::INPUT_TYPE_LENS_IMAGE:
      return ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kAddPhotoAlternateIcon
                                            : kAddPhotoAlternateOldIcon,
          ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    case omnibox::InputType::INPUT_TYPE_LENS_FILE:
      return ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kAttachFileIcon
                                            : kAttachFileOldIcon,
          ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    // The Google Drive icon is only available in Google Chrome branded builds.
    // This guard is necessary to prevent compilation errors in Chromium.
    case omnibox::InputType::INPUT_TYPE_DRIVE:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return ui::ImageModel::FromVectorIcon(
          vector_icons::kGoogleDriveMonochromeIcon, ui::kColorMenuIcon,
          ui::SimpleMenuModel::kDefaultIconSize);
#else
      return ui::ImageModel();
#endif
    default:
      return ui::ImageModel();
  }
}

const omnibox::ToolConfig* OmniboxContextMenuController::GetToolConfig(
    omnibox::ToolMode tool) const {
  auto it = std::find_if(
      input_state_.tool_configs.begin(), input_state_.tool_configs.end(),
      [&](const omnibox::ToolConfig config) { return config.tool() == tool; });
  return (it != input_state_.tool_configs.end()) ? &(*it) : nullptr;
}

std::optional<omnibox::SectionConfig>
OmniboxContextMenuController::GetToolSectionConfig() const {
  return input_state_.tools_section_config;
}

bool OmniboxContextMenuController::IsToolEnabled(omnibox::ToolMode tool) const {
  return std::none_of(
      input_state_.disabled_tools.begin(), input_state_.disabled_tools.end(),
      [&](omnibox::ToolMode disabled_tool) { return disabled_tool == tool; });
}

std::u16string OmniboxContextMenuController::GetMenuLabelForTool(
    omnibox::ToolMode tool) const {
  auto* tool_config = GetToolConfig(tool);
  if (tool_config && !tool_config->menu_label().empty()) {
    return base::UTF8ToUTF16(tool_config->menu_label());
  }

  // If the server didn't provide a menu label, return a fallback value.
  switch (tool) {
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
      return l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_CREATE_IMAGES);
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_DEEP_SEARCH);
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_CANVAS);
    default:
      return u"";
  }
}

ui::ImageModel OmniboxContextMenuController::GetIconForTool(
    omnibox::ToolMode tool) const {
  switch (tool) {
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
      return ui::ImageModel::FromResourceId(
          IDR_OMNIBOX_POPUP_IMAGES_CREATE_IMAGES_PNG);
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kTravelExploreIcon
                                            : kTravelExploreOldIcon,
          ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kDraftSparkIcon
                                            : kDraftSparkOldIcon,
          ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    default:
      return ui::ImageModel();
  }
}

const omnibox::ModelConfig* OmniboxContextMenuController::GetModelConfig(
    omnibox::ModelMode model) const {
  auto it = std::find_if(input_state_.model_configs.begin(),
                         input_state_.model_configs.end(),
                         [&](const omnibox::ModelConfig config) {
                           return config.model() == model;
                         });
  return (it != input_state_.model_configs.end()) ? &(*it) : nullptr;
}

std::optional<omnibox::SectionConfig>
OmniboxContextMenuController::GetModelSectionConfig() const {
  return input_state_.model_section_config;
}

bool OmniboxContextMenuController::IsModelEnabled(
    omnibox::ModelMode model) const {
  return std::none_of(input_state_.disabled_models.begin(),
                      input_state_.disabled_models.end(),
                      [&](omnibox::ModelMode disabled_model) {
                        return disabled_model == model;
                      });
}

std::u16string OmniboxContextMenuController::GetMenuLabelForModel(
    omnibox::ModelMode model) const {
  auto* model_config = GetModelConfig(model);
  if (model_config && !model_config->menu_label().empty()) {
    return base::UTF8ToUTF16(model_config->menu_label());
  }

  // If the server didn't provide a menu label, return a fallback value.
  switch (model) {
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_AUTO_MODEL);
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
      return l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_THINKING_3_PRO);
    default:
      return u"";
  }
}

ui::ImageModel OmniboxContextMenuController::GetIconForModel(
    omnibox::ModelMode model) const {
  switch (model) {
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kAutorenewIcon
                                            : kAutorenewOldIcon,
          ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
      return ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kBoltIcon : kBoltOldIcon,
          ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
      return ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kTimerIcon : kTimerOldIcon,
          ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    default:
      return ui::ImageModel();
  }
}

// static
OmniboxController* OmniboxContextMenuController::GetOmniboxController(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  auto* helper = OmniboxPopupWebContentsHelper::FromWebContents(web_contents);
  return helper ? helper->get_omnibox_controller() : nullptr;
}

OmniboxController* OmniboxContextMenuController::GetOmniboxController() const {
  return GetOmniboxController(web_contents_.get());
}

OmniboxEditModel* OmniboxContextMenuController::GetEditModel() {
  auto* omnibox_controller = GetOmniboxController();
  if (!omnibox_controller) {
    return nullptr;
  }
  return omnibox_controller->edit_model();
}

// static
OmniboxPopupUI* OmniboxContextMenuController::GetOmniboxPopupUI(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (auto* webui = web_contents->GetWebUI()) {
    if (auto* controller = webui->GetController()) {
      if (auto* omnibox_popup_ui = controller->GetAs<OmniboxPopupUI>()) {
        return omnibox_popup_ui;
      }
    }
  }

  // Fallback: If web_contents does not have WebUI (e.g. it is the active tab),
  // try to find it through the active browser window's LocationBar.
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser_window_interface) {
    return nullptr;
  }

  LocationBar* location_bar =
      browser_window_interface->GetFeatures().location_bar();
  if (!location_bar) {
    return nullptr;
  }

  auto* presenter_delegate = location_bar->GetPresenterDelegate();
  if (!presenter_delegate) {
    return nullptr;
  }

  auto* presenter = presenter_delegate->GetOmniboxPopupAimPresenter();
  if (!presenter) {
    return nullptr;
  }

  auto* webui_content = presenter->GetWebUIContent();
  if (!webui_content) {
    return nullptr;
  }

  auto* wrapper = webui_content->contents_wrapper();
  if (!wrapper) {
    return nullptr;
  }

  return wrapper->GetWebUIController();
}

OmniboxPopupUI* OmniboxContextMenuController::GetOmniboxPopupUI() const {
  return GetOmniboxPopupUI(web_contents_.get());
}

void OmniboxContextMenuController::ExecuteCommand(int id, int event_flags) {
  if (id == IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU) {
    // Shared tabs does not do anything as a button.
    return;
  }
  if (id == IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING) {
    if (auto* handler = GetSearchboxHandler()) {
      bool active = handler->IsSmartTabSharingActive();
      handler->SetSmartTabSharingActive(!active);

      auto* omnibox_controller = GetOmniboxController();
      bool is_aim_popup_open =
          omnibox_controller &&
          omnibox_controller->popup_state_manager()->popup_state() ==
              OmniboxPopupState::kAim;

      if (!active || is_aim_popup_open) {
        OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
      }
    }
    return;
  }
  auto* omnibox_controller = GetOmniboxController();
  bool is_aim_popup_open =
      omnibox_controller &&
      omnibox_controller->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim;
  const std::string prefix = is_aim_popup_open
                                 ? kAimContextTypeHistogramPrefix
                                 : kClassicContextTypeHistogramPrefix;
  const std::string sliced_prefix = base::StrCat({prefix, ".Clicked"});
  // Add tab context if tab is selected.
  if (IsTabCommandId(id)) {
    std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();
    int tab_index_in_menu = id - kMinOmniboxContextMenuRecentTabsCommandId;
    if (static_cast<size_t>(tab_index_in_menu) < tabs.size()) {
      // Get the tab ID based on the command ID in the simple menu.
      const auto& tab_info = tabs[tab_index_in_menu];
      bool was_uploaded = false;
      base::UnguessableToken file_token_to_delete;
      ContextualSearchboxHandler* active_handler = nullptr;
      if (auto* omnibox_popup_ui = GetOmniboxPopupUI()) {
        auto* composebox_handler = omnibox_popup_ui->composebox_handler();
        if (composebox_handler) {
          for (const auto& pair : composebox_handler->selected_tabs) {
            if (tab_info.tab_id == pair.second) {
              was_uploaded = true;
              file_token_to_delete = pair.first;
              active_handler = composebox_handler;
              break;
            }
          }
        }
        auto* omnibox_handler = omnibox_popup_ui->omnibox_handler();
        if (!was_uploaded && omnibox_handler) {
          for (const auto& pair : omnibox_handler->selected_tabs) {
            if (tab_info.tab_id == pair.second) {
              was_uploaded = true;
              file_token_to_delete = pair.first;
              active_handler = omnibox_handler;
              break;
            }
          }
        }
      }
      // If was already staged for upload, delete it, as this function was
      // called because user clicked on tab again.:
      if (was_uploaded && active_handler) {
        active_handler->DeleteContextFromBrowser(file_token_to_delete,
                                                 /*from_automatic_chip=*/false);
        // Refresh omnibox popup UI.
        OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
      } else {  // If not staged for upload, then stage for upload.
        base::UmaHistogramExactLinear(
            "ContextualSearch.ContextAdded.ContextAddedMethod.Omnibox",
            /*ContextMenu*/ 0, 4);
        if (base::FeatureList::IsEnabled(
                omnibox::kContextManagementInComposebox)) {
          base::UmaHistogramBoolean(
              "ContextualSearch.AddTabsFlyout.TabSelected.Omnibox", true);
        }
        AddTabContext(tab_info);
      }
    }
    RecordContextMenuItemSelection(sliced_prefix, id);
  } else {
    auto* omnibox_popup_ui = GetOmniboxPopupUI();
    auto* composebox_handler =
        omnibox_popup_ui ? omnibox_popup_ui->composebox_handler() : nullptr;

    bool use_input_state_model =
        base::FeatureList::IsEnabled(omnibox::kAimUsePecApi);

    bool is_file_upload_command = id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE ||
                                  id == IDC_OMNIBOX_CONTEXT_ADD_FILE;
    if (use_input_state_model) {
      if (auto it = input_type_for_command_id_.find(id);
          it != input_type_for_command_id_.end()) {
        is_file_upload_command =
            it->second == omnibox::InputType::INPUT_TYPE_LENS_IMAGE ||
            it->second == omnibox::InputType::INPUT_TYPE_LENS_FILE ||
            it->second == omnibox::InputType::INPUT_TYPE_DRIVE;
      }
    }

    if (is_aim_popup_open && is_file_upload_command) {
      if (omnibox_popup_ui && omnibox_popup_ui->popup_aim_handler()) {
        omnibox_popup_ui->popup_aim_handler()->SetPreserveContextOnClose(true);
      }
    }

    if (use_input_state_model) {
      if (auto it = input_type_for_command_id_.find(id);
          it != input_type_for_command_id_.end()) {
        if (it->second == omnibox::InputType::INPUT_TYPE_DRIVE) {
          if (composebox_handler) {
            composebox_handler->GetDriveDisclaimerStatus(base::BindOnce(
                [](base::WeakPtr<content::WebContents> web_contents,
                   bool is_aim_popup_open,
                   searchbox::mojom::DriveDisclaimerStatus status) {
                  // Abort only if the account is restricted/ineligible. If
                  // unconsented (kNotAccepted), allow the click through so
                  // OnDriveUploadClicked can trigger the ConsentKit disclaimer
                  // onboarding dialog.
                  if (status ==
                      searchbox::mojom::DriveDisclaimerStatus::kRestricted) {
                    return;
                  }
                  auto* omnibox_popup_ui =
                      GetOmniboxPopupUI(web_contents.get());
                  auto* handler = omnibox_popup_ui
                                      ? omnibox_popup_ui->composebox_handler()
                                      : nullptr;
                  if (handler) {
                    handler->OnDriveUploadClicked(
                        base::BindOnce(&OmniboxContextMenuController::
                                           HandleDriveUploadResponse,
                                       is_aim_popup_open, web_contents));
                  }
                },
                web_contents_, is_aim_popup_open));
          }
          RecordContextMenuItemSelection(sliced_prefix, id);
          return;
        }
        file_selector_->OpenFileUploadDialog(
            web_contents_.get(),
            /*is_image=*/it->second ==
                omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
            GetEditModel(),
            OmniboxPopupFileSelector::CreateImageEncodingOptions(),
            /*was_ai_mode_open=*/is_aim_popup_open);
        return;
      }

      if (auto it = tool_for_command_id_.find(id);
          it != tool_for_command_id_.end()) {
        if (composebox_handler) {
          composebox_handler->SetActiveToolMode(it->second);
          composebox_handler->RecordToolSelectionAction(it->second);
        }

        RecordContextMenuItemSelection(sliced_prefix, id);
        OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
        return;
      }

      if (auto it = model_for_command_id_.find(id);
          it != model_for_command_id_.end()) {
        if (composebox_handler) {
          composebox_handler->SetActiveModelMode(it->second);
          composebox_handler->RecordModelSelectionAction(it->second);
        }
        if (is_aim_popup_open && omnibox_popup_ui &&
            omnibox_popup_ui->popup_aim_handler()) {
          omnibox_popup_ui->popup_aim_handler()->FocusInput();
        }

        RecordContextMenuItemSelection(sliced_prefix, id);
        OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
        return;
      }
    }

    // All context actions will eventually log a histogram, but those that open
    // a dialog do so only after the dialog is closed.
    switch (id) {
      case IDC_OMNIBOX_CONTEXT_ADD_IMAGE: {
        file_selector_->OpenFileUploadDialog(
            web_contents_.get(),
            /*is_image=*/true, GetEditModel(),
            OmniboxPopupFileSelector::CreateImageEncodingOptions(),
            /*was_ai_mode_open=*/is_aim_popup_open);
        break;
      }
      case IDC_OMNIBOX_CONTEXT_ADD_FILE:
        file_selector_->OpenFileUploadDialog(
            web_contents_.get(),
            /*is_image=*/false, GetEditModel(),
            OmniboxPopupFileSelector::CreateImageEncodingOptions(),
            /*was_ai_mode_open=*/is_aim_popup_open);
        break;
      case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
        if (composebox_handler) {
          composebox_handler->SetActiveToolMode(
              omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
          composebox_handler->RecordToolSelectionAction(
              omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
        }
        RecordContextMenuItemSelection(sliced_prefix, id);
        OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
        break;
      case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
        if (composebox_handler) {
          composebox_handler->SetActiveToolMode(
              omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
          composebox_handler->RecordToolSelectionAction(
              omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
        }
        RecordContextMenuItemSelection(sliced_prefix, id);
        OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
        break;
      case IDC_OMNIBOX_CONTEXT_CANVAS:
        if (composebox_handler) {
          composebox_handler->SetActiveToolMode(
              omnibox::ToolMode::TOOL_MODE_CANVAS);
          composebox_handler->RecordToolSelectionAction(
              omnibox::ToolMode::TOOL_MODE_CANVAS);
        }
        RecordContextMenuItemSelection(sliced_prefix, id);
        OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
        break;
      default:
        NOTREACHED();
    }
  }
}

bool OmniboxContextMenuController::IsCommandIdEnabled(int command_id) const {
  // `IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU` is a structural submenu item
  // that behaves as a placeholder rather than an actionable clickable command.
  // It must be handled early to bypass the standard actionable command
  // verification logic below.
  if (command_id == IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU) {
    CHECK(
        base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox) &&
        base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox));
    if (!IsTabContextEnabled()) {
      return false;
    }
    auto* handler = GetSearchboxHandler();
    return !handler || !handler->IsSmartTabSharingActive();
  }
  if (command_id == IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING) {
    return IsTabContextEnabled();
  }
  if (command_id == ui::MenuModel::kTitleId) {
    return false;
  }

  auto* omnibox_controller = GetOmniboxController();
  if (!omnibox_controller) {
    return false;
  }

  const OmniboxPopupState page_type =
      omnibox_controller->popup_state_manager()->popup_state();
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    const std::string prefix = page_type == OmniboxPopupState::kClassic
                                   ? kClassicContextTypeHistogramPrefix
                                   : kAimContextTypeHistogramPrefix;
    const std::string sliced_prefix = base::StrCat({prefix, ".Shown"});

    // Command ID corresponds to tabs section/submenu item.
    if (IsTabCommandId(command_id)) {
      std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();
      int tab_index_in_menu =
          command_id - kMinOmniboxContextMenuRecentTabsCommandId;
      if (static_cast<size_t>(tab_index_in_menu) < tabs.size()) {
        const auto& tab_info = tabs[tab_index_in_menu];
        if (tab_info.is_checked) {
          base::UmaHistogramEnumeration(sliced_prefix,
                                        CommandIdToEnum(command_id));
          return true;
        }
      }
      bool tab_context_enabled = IsTabContextEnabled();
      if (tab_context_enabled) {
          int max_num_files = input_state_.allowed_models.empty()
                                  ? kDefaultMaxNumFiles
                                  : input_state_.max_total_inputs;
          std::vector<contextual_search::FileInfo> file_infos;
          if (auto* omnibox_popup_ui = GetOmniboxPopupUI()) {
            if (auto* session_handle =
                    omnibox_popup_ui->GetOrCreateContextualSessionHandle()) {
              file_infos = session_handle->GetUploadedContextFileInfos();
            }
          }
          if (static_cast<int>(file_infos.size()) >= max_num_files) {
            return false;
          }

        base::UmaHistogramEnumeration(sliced_prefix,
                                      CommandIdToEnum(command_id));
      }
      return tab_context_enabled;
    }

    if (auto it = input_type_for_command_id_.find(command_id);
        it != input_type_for_command_id_.end()) {
      bool input_type_enabled = input_type_info_.at(it->second).enabled;
      if (input_type_enabled) {
        base::UmaHistogramEnumeration(sliced_prefix,
                                      CommandIdToEnum(command_id));
      }
      return input_type_enabled;
    }

    if (auto it = tool_for_command_id_.find(command_id);
        it != tool_for_command_id_.end()) {
      bool tool_enabled = tool_info_.at(it->second).enabled;
      if (tool_enabled) {
        base::UmaHistogramEnumeration(sliced_prefix,
                                      CommandIdToEnum(command_id));
      }
      return tool_enabled;
    }

    if (auto it = model_for_command_id_.find(command_id);
        it != model_for_command_id_.end()) {
      bool model_enabled = model_info_.at(it->second).enabled;
      if (model_enabled) {
        base::UmaHistogramEnumeration(sliced_prefix,
                                      CommandIdToEnum(command_id));
      }
      return model_enabled;
    }

    base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(command_id));
    return true;
  }

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  if (!browser_window_interface) {
    return false;
  }

  auto* omnibox_popup_ui = GetOmniboxPopupUI();
  if (!omnibox_popup_ui || !omnibox_popup_ui->composebox_handler() ||
      !omnibox_popup_ui->composebox_handler()->input_state_model()) {
    return false;
  }

  const auto& input_state = omnibox_popup_ui->composebox_handler()
                                ->input_state_model()
                                ->GetInputState();
  const omnibox::ToolMode aim_tool_mode = input_state.active_tool;
  // If `allowed_models` is empty, the `input_state` is uninitialized and we
  // fallback to a default limit of 10. Otherwise, we use the limit provided by
  // the `input_state` even if it is 0.
  const int max_num_files = input_state.allowed_models.empty()
                                ? kDefaultMaxNumFiles
                                : input_state.max_total_inputs;

  std::vector<contextual_search::FileInfo> file_infos;
  if (auto* session_handle =
          omnibox_popup_ui->GetOrCreateContextualSessionHandle()) {
    file_infos = session_handle->GetUploadedContextFileInfos();
  }

  return IsCommandIdEnabledHelper(command_id, aim_tool_mode, file_infos,
                                  max_num_files, page_type);
}

bool OmniboxContextMenuController::IsCommandIdEnabledHelper(
    int command_id,
    omnibox::ToolMode aim_tool_mode,
    const std::vector<contextual_search::FileInfo>& file_infos,
    int max_num_files,
    OmniboxPopupState page_type) const {
  const std::string prefix = page_type == OmniboxPopupState::kClassic
                                 ? kClassicContextTypeHistogramPrefix
                                 : kAimContextTypeHistogramPrefix;
  const std::string sliced_prefix = base::StrCat({prefix, ".Shown"});

  if (IsTabCommandId(command_id)) {
    std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();
    int tab_index_in_menu =
        command_id - kMinOmniboxContextMenuRecentTabsCommandId;
    if (static_cast<size_t>(tab_index_in_menu) < tabs.size()) {
      const auto& tab_info = tabs[tab_index_in_menu];
      if (tab_info.is_checked) {
        base::UmaHistogramEnumeration(sliced_prefix,
                                      CommandIdToEnum(command_id));
        return true;
      }
    }
    auto file_upload_count = static_cast<int>(file_infos.size());
    if (file_upload_count < max_num_files) {
      base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(command_id));
    }
    return file_upload_count < max_num_files;
  }

  if (aim_tool_mode == omnibox::ToolMode::TOOL_MODE_IMAGE_GEN) {
    const bool command_enabled =
        command_id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE && file_infos.empty();
    if (command_enabled) {
      base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(command_id));
    }
    return command_enabled;
  }

  auto file_upload_count = static_cast<int>(file_infos.size());

  if (file_upload_count > 0) {
    switch (command_id) {
      case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
      case IDC_OMNIBOX_CONTEXT_CANVAS:
        return false;
      case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES: {
        const bool create_images_enabled =
            file_upload_count == 1 &&
            file_infos[0].mime_type == lens::MimeType::kImage;
        if (create_images_enabled) {
          base::UmaHistogramEnumeration(sliced_prefix,
                                        CommandIdToEnum(command_id));
        }
        return create_images_enabled;
      }
      default: {
        bool is_checked = false;
        if (IsTabCommandId(command_id)) {
          std::vector<OmniboxContextMenuController::TabInfo> tabs =
              GetRecentTabs();
          int tab_index_in_menu =
              command_id - kMinOmniboxContextMenuRecentTabsCommandId;
          if (static_cast<size_t>(tab_index_in_menu) < tabs.size()) {
            is_checked = tabs[tab_index_in_menu].is_checked;
          }
        }
        bool is_enabled = is_checked || file_upload_count < max_num_files;
        if (is_enabled) {
          base::UmaHistogramEnumeration(sliced_prefix,
                                        CommandIdToEnum(command_id));
        }
        return is_enabled;
      }
    }
  }

  base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(command_id));
  return true;
}

bool OmniboxContextMenuController::IsCommandIdVisible(int command_id) const {
  // `IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU` is a structural submenu item
  // that behaves as a placeholder rather than a clickable command. It must be
  // handled early to bypass the standard actionable command verification
  // logic below.
  if (command_id == IDC_OMNIBOX_CONTEXT_SHARED_TABS_SUBMENU) {
    CHECK(
        base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox) &&
        base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox));
    return true;
  }
  if (command_id == IDC_OMNIBOX_CONTEXT_SMART_TAB_SHARING) {
    auto* browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents_.get());
    if (!browser_window_interface) {
      return false;
    }
    return contextual_tasks::ContextualTasksContextService::
        GetIsSmartTabSharingEnabled(browser_window_interface->GetProfile());
  }

  // When using the PEC API, whether or not an item is visible is controlled
  // purely by server-side logic (see `InitializeMenuItemInfo()` for details).
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    return true;
  }

  // Command ID corresponds to tab section/submenu item.
  if (IsTabCommandId(command_id)) {
    return true;
  }

  if (command_id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE ||
      command_id == IDC_OMNIBOX_CONTEXT_ADD_FILE ||
      command_id == IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH ||
      command_id == IDC_OMNIBOX_CONTEXT_CREATE_IMAGES) {
    auto* browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents_.get());
    if (!browser_window_interface) {
      return false;
    }
    Profile* profile = browser_window_interface->GetProfile();
    if (!profile) {
      return false;
    }

    if (command_id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE ||
        command_id == IDC_OMNIBOX_CONTEXT_ADD_FILE) {
      return IsContentSharingEnabled();
    } else if (command_id == IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH) {
      return omnibox::IsDeepSearchEnabled(profile);
    } else if (command_id == IDC_OMNIBOX_CONTEXT_CREATE_IMAGES) {
      return omnibox::IsCreateImagesEnabled(profile);
    }
  }

  return true;
}

ContextualSearchboxHandler* OmniboxContextMenuController::GetSearchboxHandler()
    const {
  auto* omnibox_popup_ui = GetOmniboxPopupUI();
  if (!omnibox_popup_ui) {
    return nullptr;
  }
  if (omnibox_popup_ui->composebox_handler()) {
    return omnibox_popup_ui->composebox_handler();
  }
  return omnibox_popup_ui->omnibox_handler();
}

void OmniboxContextMenuController::OpenAiMode(
    OmniboxEditModel::AimActivation activation) {
  if (OmniboxEditModel* edit_model = GetEditModel()) {
    edit_model->OpenAiMode(activation);
  } else {
    DLOG(WARNING) << "OpenAiMode called but no edit model present.";
  }
}
