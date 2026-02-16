// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
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
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/image/image.h"

namespace {
// TODO(crbug.com/457815342): Add this to config when available.
constexpr int kMaxRecentTabs = 5;
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

OmniboxContextMenuController::ContextType CommandIdToEnum(int command_id) {
  switch (command_id) {
    case IDC_OMNIBOX_CONTEXT_ADD_IMAGE:
      return OmniboxContextMenuController::ContextType::kImage;
    case IDC_OMNIBOX_CONTEXT_ADD_FILE:
      return OmniboxContextMenuController::ContextType::kFile;
    case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
      return OmniboxContextMenuController::ContextType::kDeepResearch;
    case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
      return OmniboxContextMenuController::ContextType::kImageGen;
    case IDC_OMNIBOX_CONTEXT_CANVAS:
      return OmniboxContextMenuController::ContextType::kCanvas;
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO:
      return OmniboxContextMenuController::ContextType::kAutoModel;
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING:
      return OmniboxContextMenuController::ContextType::kThinkingModel;
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR:
      return OmniboxContextMenuController::ContextType::kRegularModel;
    default:
      // There is no command id for tabs due to there being multiple
      // tabs that would have the same command id.
      CHECK_GE(command_id, kMinOmniboxContextMenuRecentTabsCommandId);
      CHECK_LT(command_id,
               kMinOmniboxContextMenuRecentTabsCommandId + kMaxRecentTabs);
      return OmniboxContextMenuController::ContextType::kTab;
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
  auto* composebox_handler =
      GetOmniboxPopupUI() ? GetOmniboxPopupUI()->composebox_handler() : nullptr;
  if (composebox_handler &&
      base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    composebox_handler->GetInputState(
        base::BindOnce(&OmniboxContextMenuController::OnGetInputState,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  BuildMenu();
}

OmniboxContextMenuController::~OmniboxContextMenuController() = default;

void OmniboxContextMenuController::BuildMenu() {
  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    if (IsInputTypeVisible(omnibox::InputType::INPUT_TYPE_BROWSER_TAB)) {
      AddRecentTabItems();
    }
    if (IsInputTypeVisible(omnibox::InputType::INPUT_TYPE_LENS_IMAGE) ||
        IsInputTypeVisible(omnibox::InputType::INPUT_TYPE_LENS_FILE)) {
      AddSeparator();
      AddContextualInputItems();
    }
    if (!input_state_.allowed_tools.empty()) {
      AddSeparator();
      AddToolItems();
    }
    if (!input_state_.allowed_models.empty()) {
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
    AddTitleWithStringId(IDS_NTP_COMPOSE_MOST_RECENT_TABS);
  }
  std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();

  for (const auto& tab : tabs) {
    AddItemWithIcon(next_command_id_, tab.title,
                    favicon::GetDefaultFaviconModel());
    AddTabFavicon(next_command_id_, tab.url, tab.title);
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
  auto add_image_icon =
      ui::ImageModel::FromVectorIcon(kAddPhotoAlternateIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_ADD_IMAGE,
                             IDS_NTP_COMPOSE_ADD_IMAGE, add_image_icon);

  auto add_file_icon =
      ui::ImageModel::FromVectorIcon(kAttachFileIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_ADD_FILE,
                             IDS_NTP_COMPOSE_ADD_FILE, add_file_icon);
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

    auto* image_gen_config =
        GetToolConfig(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
    AddItemWithIcon(IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
                    base::UTF8ToUTF16(
                        image_gen_config ? image_gen_config->menu_label() : ""),
                    create_images_icon);

    auto* deep_search_config =
        GetToolConfig(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
    AddItemWithIcon(
        IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
        base::UTF8ToUTF16(deep_search_config ? deep_search_config->menu_label()
                                             : ""),
        deep_search_icon);

    auto canvas_icon =
        ui::ImageModel::FromVectorIcon(kDraftSparkIcon, ui::kColorMenuIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize);
    auto* canvas_config = GetToolConfig(omnibox::ToolMode::TOOL_MODE_CANVAS);
    AddItemWithIcon(
        IDC_OMNIBOX_CONTEXT_CANVAS,
        base::UTF8ToUTF16(canvas_config ? canvas_config->menu_label() : ""),
        canvas_icon);
  } else {
    AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
                               IDS_NTP_COMPOSE_CREATE_IMAGES,
                               create_images_icon);

    AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
                               IDS_NTP_COMPOSE_DEEP_SEARCH, deep_search_icon);
  }
}

void OmniboxContextMenuController::AddModelPickerItems() {
  auto model_section_config = GetModelSectionConfig();
  if (omnibox::kShowContextMenuHeaders.Get() && model_section_config &&
      !model_section_config->header().empty()) {
    menu_model_->AddTitle(base::UTF8ToUTF16(model_section_config->header()));
  }

  auto* auto_model_config =
      GetModelConfig(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
  auto auto_model_label = base::UTF8ToUTF16(
      auto_model_config ? auto_model_config->menu_label() : "");

  auto* regular_model_config =
      GetModelConfig(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  auto regular_model_label = base::UTF8ToUTF16(
      regular_model_config ? regular_model_config->menu_label() : "");

  auto* thinking_model_config =
      GetModelConfig(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  auto thinking_model_label = base::UTF8ToUTF16(
      thinking_model_config ? thinking_model_config->menu_label() : "");

  auto check_icon = ui::ImageModel::FromVectorIcon(
      kCheckIcon, ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
  bool is_aim_popup_open =
      GetOmniboxController() &&
      GetOmniboxController()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim;

  auto auto_model_icon =
      ui::ImageModel::FromVectorIcon(kAutorenewIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithIcon(
      IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO, auto_model_label,
      is_aim_popup_open &&
              input_state_.active_model ==
                  omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE
          ? check_icon
          : auto_model_icon);

  auto regular_model_icon = ui::ImageModel::FromVectorIcon(
      kBoltIcon, ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithIcon(
      IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR, regular_model_label,
      is_aim_popup_open && input_state_.active_model ==
                               omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR
          ? check_icon
          : regular_model_icon);

  auto thinking_model_icon = ui::ImageModel::FromVectorIcon(
      kTimerIcon, ui::kColorMenuIcon, ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithIcon(
      IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING, thinking_model_label,
      is_aim_popup_open && input_state_.active_model ==
                               omnibox::ModelMode::MODEL_MODE_GEMINI_PRO
          ? check_icon
          : thinking_model_icon);
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
      std::min(static_cast<int>(tabs.size()), kMaxRecentTabs);
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

omnibox::InputType OmniboxContextMenuController::GetInputTypeForCommandId(
    int command_id) const {
  // Command ID corresponds to "Most recent tabs" menu item.
  if (command_id >= kMinOmniboxContextMenuRecentTabsCommandId &&
      command_id < next_command_id_) {
    return omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
  }

  switch (command_id) {
    case IDC_OMNIBOX_CONTEXT_ADD_IMAGE:
      return omnibox::InputType::INPUT_TYPE_LENS_IMAGE;
    case IDC_OMNIBOX_CONTEXT_ADD_FILE:
      return omnibox::InputType::INPUT_TYPE_LENS_FILE;
    default:
      NOTREACHED();
  }
}

bool OmniboxContextMenuController::IsInputTypeVisible(
    omnibox::InputType input_type) const {
  return std::any_of(input_state_.allowed_input_types.begin(),
                     input_state_.allowed_input_types.end(),
                     [&](omnibox::InputType allowed_input_type) {
                       return allowed_input_type == input_type;
                     });
}

bool OmniboxContextMenuController::IsInputTypeEnabled(
    omnibox::InputType input_type) const {
  return std::none_of(input_state_.disabled_input_types.begin(),
                      input_state_.disabled_input_types.end(),
                      [&](omnibox::InputType disabled_input_type) {
                        return disabled_input_type == input_type;
                      });
}

omnibox::ToolMode OmniboxContextMenuController::GetToolModeForCommandId(
    int command_id) const {
  switch (command_id) {
    case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
      return omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH;
    case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
      return omnibox::ToolMode::TOOL_MODE_IMAGE_GEN;
    case IDC_OMNIBOX_CONTEXT_CANVAS:
      return omnibox::ToolMode::TOOL_MODE_CANVAS;
    default:
      NOTREACHED();
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

bool OmniboxContextMenuController::IsToolVisible(omnibox::ToolMode tool) const {
  return std::any_of(
      input_state_.allowed_tools.begin(), input_state_.allowed_tools.end(),
      [&](omnibox::ToolMode allowed_tool) { return allowed_tool == tool; });
}

bool OmniboxContextMenuController::IsToolEnabled(omnibox::ToolMode tool) const {
  return std::none_of(
      input_state_.disabled_tools.begin(), input_state_.disabled_tools.end(),
      [&](omnibox::ToolMode disabled_tool) { return disabled_tool == tool; });
}

omnibox::ModelMode OmniboxContextMenuController::GetModelModeForCommandId(
    int command_id) const {
  switch (command_id) {
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE;
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR;
    case IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO;
    default:
      NOTREACHED();
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

bool OmniboxContextMenuController::IsModelVisible(
    omnibox::ModelMode model) const {
  return std::any_of(
      input_state_.allowed_models.begin(), input_state_.allowed_models.end(),
      [&](omnibox::ModelMode allowed_model) { return allowed_model == model; });
}

bool OmniboxContextMenuController::IsModelEnabled(
    omnibox::ModelMode model) const {
  return std::none_of(input_state_.disabled_models.begin(),
                      input_state_.disabled_models.end(),
                      [&](omnibox::ModelMode disabled_model) {
                        return disabled_model == model;
                      });
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
      id < next_command_id_) {
    std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();
    int tab_index_in_menu = id - kMinOmniboxContextMenuRecentTabsCommandId;
    if (static_cast<size_t>(tab_index_in_menu) < tabs.size()) {
      const auto& tab_info = tabs[tab_index_in_menu];
      AddTabContext(tab_info);
    }
    base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(id));
  } else {
    bool is_file_upload_command = id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE ||
                                  id == IDC_OMNIBOX_CONTEXT_ADD_FILE;

    auto omnibox_popup_ui = GetOmniboxPopupUI();
    if (is_aim_popup_open && is_file_upload_command) {
      if (omnibox_popup_ui && omnibox_popup_ui->popup_aim_handler()) {
        omnibox_popup_ui->popup_aim_handler()->SetPreserveContextOnClose(true);
      }
    }
    auto* composebox_handler =
        omnibox_popup_ui ? omnibox_popup_ui->composebox_handler() : nullptr;
    bool use_input_state_model =
        base::FeatureList::IsEnabled(omnibox::kAimUsePecApi) &&
        composebox_handler;

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
      case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
        if (use_input_state_model) {
          composebox_handler->SetActiveToolMode(
              omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
        }
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/searchbox::mojom::ToolMode::kDeepSearch);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(id));
        break;
      case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
        if (use_input_state_model) {
          composebox_handler->SetActiveToolMode(
              omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
        }
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/searchbox::mojom::ToolMode::kCreateImage);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(id));
        break;
      case IDC_OMNIBOX_CONTEXT_CANVAS:
        if (use_input_state_model) {
          composebox_handler->SetActiveToolMode(
              omnibox::ToolMode::TOOL_MODE_CANVAS);
        }
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/searchbox::mojom::ToolMode::kCanvas);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        base::UmaHistogramEnumeration(sliced_prefix, CommandIdToEnum(id));
        break;
      case IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO:
        if (use_input_state_model) {
          composebox_handler->SetActiveModelMode(
              omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
        }
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        break;
      case IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR:
        if (use_input_state_model) {
          composebox_handler->SetActiveModelMode(
              omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
        }
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        break;
      case IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING:
        if (use_input_state_model) {
          composebox_handler->SetActiveModelMode(
              omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
        }
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        break;
      default:
        NOTREACHED();
    }
  }
}

bool OmniboxContextMenuController::IsCommandIdEnabledHelper(
    int command_id,
    omnibox::ToolMode aim_tool_mode,
    const std::vector<contextual_search::FileInfo>& file_infos,
    int max_num_files,
    OmniboxPopupState page_type) {
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

bool OmniboxContextMenuController::IsCommandIdEnabled(int command_id) const {
  if (command_id == ui::MenuModel::kTitleId) {
    return false;
  }

  if (base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    // Command ID corresponds to "Most recent tabs" menu item.
    if (command_id >= kMinOmniboxContextMenuRecentTabsCommandId &&
        command_id < next_command_id_) {
      return IsInputTypeEnabled(GetInputTypeForCommandId(command_id));
    }

    switch (command_id) {
      case IDC_OMNIBOX_CONTEXT_ADD_IMAGE:
      case IDC_OMNIBOX_CONTEXT_ADD_FILE:
        return IsInputTypeEnabled(GetInputTypeForCommandId(command_id));
      case IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH:
      case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
      case IDC_OMNIBOX_CONTEXT_CANVAS:
        return IsToolEnabled(GetToolModeForCommandId(command_id));
      case IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO:
      case IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR:
      case IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING:
        return IsModelEnabled(GetModelModeForCommandId(command_id));
      default:
        return true;
    }
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

  auto omnibox_controller = GetOmniboxController();
  if (!omnibox_controller) {
    return false;
  }

  return IsCommandIdEnabledHelper(
      command_id, aim_tool_mode, file_infos, max_num_files,
      omnibox_controller->popup_state_manager()->popup_state());
}

bool OmniboxContextMenuController::IsCommandIdVisible(int command_id) const {
  // Command ID corresponds to "Most recent tabs" menu item.
  if (command_id >= kMinOmniboxContextMenuRecentTabsCommandId &&
      command_id < next_command_id_) {
    return base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)
               ? IsInputTypeVisible(GetInputTypeForCommandId(command_id))
               : true;
  }

  if (command_id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE ||
      command_id == IDC_OMNIBOX_CONTEXT_ADD_FILE ||
      command_id == IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH ||
      command_id == IDC_OMNIBOX_CONTEXT_CREATE_IMAGES ||
      command_id == IDC_OMNIBOX_CONTEXT_CANVAS ||
      command_id == IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO ||
      command_id == IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR ||
      command_id == IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING) {
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
      return base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)
                 ? IsInputTypeVisible(GetInputTypeForCommandId(command_id))
                 : IsContentSharingEnabled();
    } else if (command_id == IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH) {
      return base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)
                 ? IsToolVisible(GetToolModeForCommandId(command_id))
                 : omnibox::IsDeepSearchEnabled(profile);
    } else if (command_id == IDC_OMNIBOX_CONTEXT_CREATE_IMAGES) {
      return base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)
                 ? IsToolVisible(GetToolModeForCommandId(command_id))
                 : omnibox::IsCreateImagesEnabled(profile);
    } else if (command_id == IDC_OMNIBOX_CONTEXT_CANVAS) {
      return IsToolVisible(GetToolModeForCommandId(command_id));
    } else if (command_id == IDC_OMNIBOX_CONTEXT_SET_MODEL_AUTO ||
               command_id == IDC_OMNIBOX_CONTEXT_SET_MODEL_REGULAR ||
               command_id == IDC_OMNIBOX_CONTEXT_SET_MODEL_THINKING) {
      return IsModelVisible(GetModelModeForCommandId(command_id));
    }
  }

  return true;
}
