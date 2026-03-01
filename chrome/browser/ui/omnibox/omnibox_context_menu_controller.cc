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
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
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
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/model_config.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/section_config.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/image/image.h"

namespace {
constexpr int kMinOmniboxContextMenuRecentTabsCommandId = 33000;

bool IsValidTab(GURL url) {
  // Skip tabs that are still loading, and skip webui.
  return url.is_valid() && !url.is_empty() &&
         !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeUIUntrustedScheme) &&
         !url.IsAboutBlank();
}

std::optional<lens::ImageEncodingOptions> CreateImageEncodingOptions() {
  // TODO(crbug.com/457815342): Use omnibox fieldtrial when available.
  auto image_upload_config =
      omnibox::FeatureConfig::Get().config.composebox().image_upload();
  return lens::ImageEncodingOptions{
      .enable_webp_encoding = image_upload_config.enable_webp_encoding(),
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};
}

bool IsThinkingModel(omnibox::ModelMode model) {
  return model == omnibox::ModelMode::MODEL_MODE_GEMINI_PRO ||
         model == omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;
}

searchbox::mojom::ToolMode GetSearchboxToolMode(omnibox::ToolMode tool) {
  switch (tool) {
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
      return searchbox::mojom::ToolMode::kCreateImage;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return searchbox::mojom::ToolMode::kDeepSearch;
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return searchbox::mojom::ToolMode::kCanvas;
    default:
      return searchbox::mojom::ToolMode::kDefault;
  }
}
}  // namespace

OmniboxContextMenuController::OmniboxContextMenuController(
    OmniboxPopupFileSelector* file_selector,
    content::WebContents* web_contents)
    : file_selector_(file_selector->GetWeakPtr()),
      web_contents_(web_contents->GetWeakPtr()) {
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  next_command_id_ = kMinOmniboxContextMenuRecentTabsCommandId;
  min_tools_and_models_command_id_ =
      kMinOmniboxContextMenuRecentTabsCommandId +
      omnibox::kContextMenuMaxTabSuggestions.Get();
  auto* composebox_handler =
      GetOmniboxPopupUI() ? GetOmniboxPopupUI()->composebox_handler() : nullptr;
  if (composebox_handler &&
      base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    composebox_handler->GetInputState(
        base::BindOnce(&OmniboxContextMenuController::OnGetInputState,
                       weak_ptr_factory_.GetWeakPtr()));
    InitializeMenuItemInfo();
  }
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

  if (omnibox::kShowContextMenuHeaders.Get()) {
    AddTitleWithStringId(IDS_NTP_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE);
  }
  std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();

  for (const auto& tab : tabs) {
    AddItemWithIcon(next_command_id_, tab.title,
                    favicon::GetDefaultFaviconModel());
    AddTabFavicon(next_command_id_, tab.url, tab.title);
    input_type_for_command_id_[next_command_id_] =
        omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
    next_command_id_ += 1;
  }
  // Remove header if no tabs to show.
  if (tabs.empty()) {
    auto index = menu_model_->GetIndexOfCommandId(ui::MenuModel::kTitleId);
    if (index) {
      menu_model_->RemoveItemAt(index.value());
      return;
    }
  }
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
      input_type_for_command_id_[next_command_id_] = input_type;
      next_command_id_++;
    }
    min_tools_and_models_command_id_ = next_command_id_;
  } else {
    auto add_image_icon = ui::ImageModel::FromVectorIcon(
        kAddPhotoAlternateIcon, ui::kColorMenuIcon,
        ui::SimpleMenuModel::kDefaultIconSize);
    AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
                               IDS_NTP_COMPOSE_ADD_IMAGE, add_image_icon);

    auto add_file_icon =
        ui::ImageModel::FromVectorIcon(kAttachFileIcon, ui::kColorMenuIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize);
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
  auto deep_search_icon =
      ui::ImageModel::FromVectorIcon(kTravelExploreIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize);

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

  const bool thinking_icon_update_enabled =
      base::FeatureList::IsEnabled(omnibox::kThinkingModelIconUpdate);
  const bool has_thinking_model =
      model_info_.find(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO) !=
      model_info_.end();
  const bool has_pro_no_gen_ui_model =
      model_info_.find(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI) !=
      model_info_.end();
  const bool use_new_thinking_icon = thinking_icon_update_enabled &&
                                     has_thinking_model &&
                                     has_pro_no_gen_ui_model;
  auto thinking_model_icon = ui::ImageModel::FromVectorIcon(
      use_new_thinking_icon ? kAstrophotographyModeIcon : kTimerIcon,
      ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);

  auto check_icon = ui::ImageModel::FromVectorIcon(
      kCheckIcon, ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);

  next_command_id_ = min_tools_and_models_command_id_;
  for (const auto model : input_state_.allowed_models) {
    auto& menu_item_info = model_info_[model];
    const auto& menu_icon =
        IsThinkingModel(model) ? thinking_model_icon : menu_item_info.menu_icon;
    AddItemWithIcon(next_command_id_, menu_item_info.menu_label,
                    is_aim_popup_open && input_state_.active_model == model
                        ? check_icon
                        : menu_icon);
    model_for_command_id_[next_command_id_] = model;
    next_command_id_++;
  }
  min_tools_and_models_command_id_ = next_command_id_;
}

std::vector<OmniboxContextMenuController::TabInfo>
OmniboxContextMenuController::GetRecentTabs() {
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
  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  for (tabs::TabInterface* tab : *tab_strip_model) {
    content::WebContents* web_contents = tab->GetContents();
    const auto& last_committed_url = web_contents->GetLastCommittedURL();
    if (!IsValidTab(last_committed_url)) {
      continue;
    }
    if (std::ranges::any_of(uploaded_file_infos, [&](const auto& info) {
          return last_committed_url == info.tab_url;
        })) {
      continue;
    }

    OmniboxContextMenuController::TabInfo tab_data;
    tab_data.tab_id = tab->GetHandle().raw_value();
    tab_data.title = TabUIHelper::From(tab)->GetTitle();
    tab_data.url = last_committed_url;

    tab_data.last_active =
        std::max(web_contents->GetLastActiveTimeTicks(),
                 web_contents->GetLastInteractionTimeTicks());
    tabs.push_back(tab_data);
  }

  // Sort tabs by most recently active.
  int max_tab_suggestions =
      std::min(static_cast<int>(tabs.size()),
               omnibox::kContextMenuMaxTabSuggestions.Get());
  std::partial_sort(tabs.begin(), tabs.begin() + max_tab_suggestions,
                    tabs.end(),
                    [](const OmniboxContextMenuController::TabInfo& a,
                       const OmniboxContextMenuController::TabInfo& b) {
                      return a.last_active > b.last_active;
                    });
  tabs.resize(max_tab_suggestions);
  return tabs;
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

  const std::optional<size_t> index_in_menu =
      menu_model_->GetIndexOfCommandId(command_id);
  DCHECK(index_in_menu.has_value());
  menu_model_->SetIcon(index_in_menu.value(),
                       ui::ImageModel::FromImage(image_result.image));
  if (menu_model_->menu_model_delegate()) {
    menu_model_->menu_model_delegate()->OnIconChanged(command_id);
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
  UpdateSearchboxContext(/*tab_info=*/tab_info, /*tool_mode=*/std::nullopt);
  GetEditModel()->OpenAiMode(/*via_keyboard=*/false, /*via_context_menu=*/true);
}

void OmniboxContextMenuController::UpdateSearchboxContext(
    std::optional<TabInfo> tab_info,
    std::optional<searchbox::mojom::ToolMode> tool_mode) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
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

  if (tool_mode) {
    context->mode = *tool_mode;
  }

  auto omnibox_controller = GetOmniboxController();

  if (omnibox_controller &&
      omnibox_controller->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim) {
    auto omnibox_popup_ui = GetOmniboxPopupUI();
    if (omnibox_popup_ui && omnibox_popup_ui->popup_aim_handler()) {
      omnibox_popup_ui->popup_aim_handler()->AddContext(std::move(context));
    }
  } else {
    searchbox_context_data->SetPendingContext(std::move(context));
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

OmniboxContextMenuController::ContextType
OmniboxContextMenuController::CommandIdToEnum(int command_id) const {
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    if (auto it = input_type_for_command_id_.find(command_id);
        it != input_type_for_command_id_.end()) {
      switch (it->second) {
        case omnibox::InputType::INPUT_TYPE_BROWSER_TAB:
          return OmniboxContextMenuController::ContextType::kTab;
        case omnibox::InputType::INPUT_TYPE_LENS_IMAGE:
          return OmniboxContextMenuController::ContextType::kImage;
        case omnibox::InputType::INPUT_TYPE_LENS_FILE:
          return OmniboxContextMenuController::ContextType::kFile;
        default:
          return OmniboxContextMenuController::ContextType::kUnknown;
      }
    }

    if (auto it = tool_for_command_id_.find(command_id);
        it != tool_for_command_id_.end()) {
      switch (it->second) {
        case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
          return OmniboxContextMenuController::ContextType::kImageGen;
        case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
          return OmniboxContextMenuController::ContextType::kDeepResearch;
        case omnibox::ToolMode::TOOL_MODE_CANVAS:
          return OmniboxContextMenuController::ContextType::kCanvas;
        default:
          return OmniboxContextMenuController::ContextType::kUnknown;
      }
    }

    if (auto it = model_for_command_id_.find(command_id);
        it != model_for_command_id_.end()) {
      switch (it->second) {
        case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
          return OmniboxContextMenuController::ContextType::kAutoModel;
        case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
          return OmniboxContextMenuController::ContextType::kRegularModel;
        case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
          return OmniboxContextMenuController::ContextType::kThinkingModel;
        case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
          return OmniboxContextMenuController::ContextType::kProNoGenUiModel;
        default:
          return OmniboxContextMenuController::ContextType::kUnknown;
      }
    }
  }

  switch (command_id) {
    case IDC_OMNIBOX_CONTEXT_ADD_IMAGE:
      return OmniboxContextMenuController::ContextType::kImage;
    case IDC_OMNIBOX_CONTEXT_ADD_FILE:
      return OmniboxContextMenuController::ContextType::kFile;
    case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
      return OmniboxContextMenuController::ContextType::kImageGen;
    case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
      return OmniboxContextMenuController::ContextType::kDeepResearch;
    default:
      // There is no command id for tabs due to there being multiple
      // tabs that would have the same command id.
      CHECK_GE(command_id, kMinOmniboxContextMenuRecentTabsCommandId);
      CHECK_LT(command_id, kMinOmniboxContextMenuRecentTabsCommandId +
                               omnibox::kContextMenuMaxTabSuggestions.Get());
      return OmniboxContextMenuController::ContextType::kTab;
  }
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
    default:
      return u"";
  }
}

ui::ImageModel OmniboxContextMenuController::GetIconForInputType(
    omnibox::InputType input_type) const {
  switch (input_type) {
    case omnibox::InputType::INPUT_TYPE_LENS_IMAGE:
      return ui::ImageModel::FromVectorIcon(
          kAddPhotoAlternateIcon, ui::kColorMenuIcon,
          ui::SimpleMenuModel::kDefaultIconSize);
    case omnibox::InputType::INPUT_TYPE_LENS_FILE:
      return ui::ImageModel::FromVectorIcon(
          kAttachFileIcon, ui::kColorMenuIcon,
          ui::SimpleMenuModel::kDefaultIconSize);
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
          kTravelExploreIcon, ui::kColorMenuIcon,
          ui::SimpleMenuModel::kDefaultIconSize);
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return ui::ImageModel::FromVectorIcon(
          kDraftSparkIcon, ui::kColorMenuIcon,
          ui::SimpleMenuModel::kDefaultIconSize);
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
          kAutorenewIcon, ui::kColorMenuIcon,
          ui::SimpleMenuModel::kDefaultIconSize);
    case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
      return ui::ImageModel::FromVectorIcon(
          kBoltIcon, ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI:
      return ui::ImageModel::FromVectorIcon(
          kTimerIcon, ui::kColorMenuIcon,
          ui::SimpleMenuModel::kDefaultIconSize);
    default:
      return ui::ImageModel();
  }
}

raw_ptr<OmniboxController> OmniboxContextMenuController::GetOmniboxController()
    const {
  auto* helper =
      OmniboxPopupWebContentsHelper::FromWebContents(web_contents_.get());
  return helper->get_omnibox_controller();
}

raw_ptr<OmniboxEditModel> OmniboxContextMenuController::GetEditModel() {
  auto omnibox_controller = GetOmniboxController();
  if (!omnibox_controller) {
    return nullptr;
  }
  return omnibox_controller->edit_model();
}

raw_ptr<OmniboxPopupUI> OmniboxContextMenuController::GetOmniboxPopupUI()
    const {
  if (auto* webui = web_contents_->GetWebUI()) {
    return webui->GetController()->GetAs<OmniboxPopupUI>();
  }
  return nullptr;
}

void OmniboxContextMenuController::ExecuteCommand(int id, int event_flags) {
  auto omnibox_controller = GetOmniboxController();
  bool is_aim_popup_open =
      omnibox_controller &&
      omnibox_controller->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim;
  const std::string prefix = is_aim_popup_open
                                 ? kAimContextTypeHistogramPrefix
                                 : kClassicContextTypeHistogramPrefix;
  const std::string sliced_prefix = base::StrCat({prefix, ".Clicked"});
  // Add tab context if tab is selected.
  if (id >= kMinOmniboxContextMenuRecentTabsCommandId &&
      id < kMinOmniboxContextMenuRecentTabsCommandId +
               omnibox::kContextMenuMaxTabSuggestions.Get()) {
    base::UmaHistogramExactLinear(
        "ContextualSearch.ContextAdded.ContextAddedMethod.Omnibox",
        /*ContextMenu*/ 0, 4);
    std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();
    int tab_index_in_menu = id - kMinOmniboxContextMenuRecentTabsCommandId;
    if (static_cast<size_t>(tab_index_in_menu) < tabs.size()) {
      const auto& tab_info = tabs[tab_index_in_menu];
      AddTabContext(tab_info);
    }
    base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(id));
  } else {
    auto omnibox_popup_ui = GetOmniboxPopupUI();
    auto* composebox_handler =
        omnibox_popup_ui ? omnibox_popup_ui->composebox_handler() : nullptr;

    bool use_input_state_model =
        base::FeatureList::IsEnabled(omnibox::kAimUsePecApi) &&
        composebox_handler;

    bool is_file_upload_command = id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE ||
                                  id == IDC_OMNIBOX_CONTEXT_ADD_FILE;
    if (use_input_state_model) {
      if (auto it = input_type_for_command_id_.find(id);
          it != input_type_for_command_id_.end()) {
        is_file_upload_command =
            it->second == omnibox::InputType::INPUT_TYPE_LENS_IMAGE ||
            it->second == omnibox::InputType::INPUT_TYPE_LENS_FILE;
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
        file_selector_->OpenFileUploadDialog(
            web_contents_.get(),
            /*is_image=*/it->second ==
                omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
            GetEditModel(), CreateImageEncodingOptions(),
            /*was_ai_mode_open=*/is_aim_popup_open);
        return;
      }

      if (auto it = tool_for_command_id_.find(id);
          it != tool_for_command_id_.end()) {
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/GetSearchboxToolMode(it->second));
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        base::UmaHistogramEnumeration(sliced_prefix,
                                      CommandIdToEnum(it->first));
        return;
      }

      if (auto it = model_for_command_id_.find(id);
          it != model_for_command_id_.end()) {
        composebox_handler->SetActiveModelMode(it->second);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        base::UmaHistogramEnumeration(sliced_prefix,
                                      CommandIdToEnum(it->first));
        return;
      }
    }

    // All context actions will eventually log a histogram, but those that open
    // a dialog do so only after the dialog is closed.
    switch (id) {
      case IDC_OMNIBOX_CONTEXT_ADD_IMAGE: {
        file_selector_->OpenFileUploadDialog(
            web_contents_.get(),
            /*is_image=*/true, GetEditModel(), CreateImageEncodingOptions(),
            /*was_ai_mode_open=*/is_aim_popup_open);
        break;
      }
      case IDC_OMNIBOX_CONTEXT_ADD_FILE:
        file_selector_->OpenFileUploadDialog(
            web_contents_.get(),
            /*is_image=*/false, GetEditModel(), CreateImageEncodingOptions(),
            /*was_ai_mode_open=*/is_aim_popup_open);
        break;
      case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/searchbox::mojom::ToolMode::kCreateImage);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(id));
        break;
      case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/searchbox::mojom::ToolMode::kDeepSearch);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(id));
        break;
      case IDC_OMNIBOX_CONTEXT_CANVAS:
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/searchbox::mojom::ToolMode::kCanvas);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(id));
        break;
      default:
        NOTREACHED();
    }
  }
}

bool OmniboxContextMenuController::IsCommandIdEnabled(int command_id) const {
  if (command_id == ui::MenuModel::kTitleId) {
    return false;
  }

  auto omnibox_controller = GetOmniboxController();
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

    // Command ID corresponds to "Most recent tabs" menu item.
    if (command_id >= kMinOmniboxContextMenuRecentTabsCommandId &&
        command_id < kMinOmniboxContextMenuRecentTabsCommandId +
                         omnibox::kContextMenuMaxTabSuggestions.Get()) {
      auto it =
          input_type_info_.find(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
      return it != input_type_info_.end() && it->second.enabled;
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

  auto omnibox_popup_ui = GetOmniboxPopupUI();
  if (!omnibox_popup_ui || !omnibox_popup_ui->composebox_handler()) {
    return false;
  }

  const omnibox::ToolMode aim_tool_mode =
      omnibox_popup_ui->composebox_handler()->GetInputState().active_tool;

  auto* session_handle = omnibox_popup_ui->GetOrCreateContextualSessionHandle();
  std::vector<contextual_search::FileInfo> file_infos;
  if (session_handle) {
    file_infos = session_handle->GetUploadedContextFileInfos();
  }
  auto max_num_files =
      omnibox::FeatureConfig::Get().config.composebox().max_num_files();

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
      default:
        if (file_upload_count < max_num_files) {
          base::UmaHistogramEnumeration(sliced_prefix,
                                        CommandIdToEnum(command_id));
        }
        return file_upload_count < max_num_files;
    }
  }

  base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(command_id));
  return true;
}

bool OmniboxContextMenuController::IsCommandIdVisible(int command_id) const {
  // When using the PEC API, whether or not an item is visible is controlled
  // purely by server-side logic (see `InitializeMenuItemInfo()` for details).
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    return true;
  }

  // Command ID corresponds to "Most recent tabs" menu item.
  if (command_id >= kMinOmniboxContextMenuRecentTabsCommandId &&
      command_id < kMinOmniboxContextMenuRecentTabsCommandId +
                       omnibox::kContextMenuMaxTabSuggestions.Get()) {
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
