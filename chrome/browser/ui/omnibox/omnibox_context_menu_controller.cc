// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
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
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/lens/contextual_input.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
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
}  // namespace

OmniboxContextMenuController::OmniboxContextMenuController(
    OmniboxPopupFileSelector* file_selector,
    content::WebContents* web_contents)
    : file_selector_(file_selector->GetWeakPtr()),
      web_contents_(web_contents->GetWeakPtr()) {
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  next_command_id_ = kMinOmniboxContextMenuRecentTabsCommandId;
  BuildMenu();
}

OmniboxContextMenuController::~OmniboxContextMenuController() = default;

void OmniboxContextMenuController::BuildMenu() {
  AddRecentTabItems();
  AddStaticItems();
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
  AddTitleWithStringId(IDS_NTP_COMPOSE_MOST_RECENT_TABS);
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
  AddSeparator();
}

void OmniboxContextMenuController::AddStaticItems() {
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

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  Profile* profile = browser_window_interface->GetProfile();

  if (omnibox::IsDeepSearchEnabled(profile) ||
      omnibox::IsCreateImagesEnabled(profile)) {
    AddSeparator();
  }

  auto deep_search_icon =
      ui::ImageModel::FromVectorIcon(kTravelExploreIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize);
  AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH,
                             IDS_NTP_COMPOSE_DEEP_SEARCH, deep_search_icon);
  auto create_images_icon = ui::ImageModel::FromResourceId(
      IDR_OMNIBOX_POPUP_IMAGES_CREATE_IMAGES_PNG);
  AddItemWithStringIdAndIcon(IDC_OMNIBOX_CONTEXT_CREATE_IMAGES,
                             IDS_NTP_COMPOSE_CREATE_IMAGES, create_images_icon);
}

std::vector<OmniboxContextMenuController::TabInfo>
OmniboxContextMenuController::GetRecentTabs() {
  std::vector<OmniboxContextMenuController::TabInfo> tabs;

  // Iterate through the tab strip model.
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  for (int i = 0; i < tab_strip_model->count(); i++) {
    tabs::TabInterface* const tab = tab_strip_model->GetTabAtIndex(i);
    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, i);
    const auto& last_committed_url = tab_renderer_data.last_committed_url;
    if (!IsValidTab(last_committed_url)) {
      continue;
    }

    OmniboxContextMenuController::TabInfo tab_data;
    tab_data.tab_id = tab->GetHandle().raw_value();
    tab_data.title = tab_renderer_data.title;
    tab_data.url = last_committed_url;

    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
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
    auto tab_attachment = searchbox::mojom::TabAttachmentStub::New();
    tab_attachment->tab_id = tab_info->tab_id;
    tab_attachment->title = base::UTF16ToUTF8(tab_info->title);
    tab_attachment->url = tab_info->url;
    context->file_infos.push_back(
        searchbox::mojom::SearchContextAttachmentStub::NewTabAttachment(
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
  // Add tab context if tab is selected.
  if (id >= kMinOmniboxContextMenuRecentTabsCommandId &&
      id < next_command_id_) {
    std::vector<OmniboxContextMenuController::TabInfo> tabs = GetRecentTabs();
    int tab_index_in_menu = id - kMinOmniboxContextMenuRecentTabsCommandId;
    if (static_cast<size_t>(tab_index_in_menu) < tabs.size()) {
      const auto& tab_info = tabs[tab_index_in_menu];
      AddTabContext(tab_info);
    }
  } else {
    auto omnibox_controller = GetOmniboxController();
    bool is_aim_popup_open =
        omnibox_controller &&
        omnibox_controller->popup_state_manager()->popup_state() ==
            OmniboxPopupState::kAim;

    bool is_file_upload_command = id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE ||
                                  id == IDC_OMNIBOX_CONTEXT_ADD_FILE;

    if (is_aim_popup_open && is_file_upload_command) {
      auto omnibox_popup_ui = GetOmniboxPopupUI();
      if (omnibox_popup_ui && omnibox_popup_ui->popup_aim_handler()) {
        omnibox_popup_ui->popup_aim_handler()->SetPreserveContextOnClose(true);
      }
    }

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
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/searchbox::mojom::ToolMode::kDeepSearch);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
        break;
      case IDC_OMNIBOX_CONTEXT_CREATE_IMAGES:
        UpdateSearchboxContext(
            /*tab_info=*/std::nullopt,
            /*tool_mode=*/searchbox::mojom::ToolMode::kCreateImage);
        GetEditModel()->OpenAiMode(/*via_keyboard=*/false,
                                   /*via_context_menu=*/true);
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

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  if (!browser_window_interface) {
    return false;
  }

  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents_.get());
  if (!helper) {
    return false;
  }
  auto* handle = helper->session_handle();
  if (!handle) {
    return false;
  }

  auto omnibox_popup_ui = GetOmniboxPopupUI();
  if (!omnibox_popup_ui || !omnibox_popup_ui->composebox_handler()) {
    return false;
  }

  const omnibox::ChromeAimToolsAndModels aim_tool_mode =
      omnibox_popup_ui->composebox_handler()->GetAimToolMode();
  if (aim_tool_mode == omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN) {
    return command_id == IDC_OMNIBOX_CONTEXT_ADD_IMAGE;
  }

  auto file_upload_count =
      static_cast<int>(handle->GetUploadedContextTokens().size());
  if (file_upload_count > 0) {
    auto max_num_files =
        omnibox::FeatureConfig::Get().config.composebox().max_num_files();
    if (file_upload_count < max_num_files) {
      return command_id != IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH;
    } else {
      // Note: If a file is added, create images should be disabled but this
      // is handled in the WebUI by disabling the button. This will need to be
      // updated when multifile upload is added.
      return command_id == IDC_OMNIBOX_CONTEXT_CREATE_IMAGES;
    }
  }

  return true;
}

bool OmniboxContextMenuController::IsCommandIdVisible(int command_id) const {
  if (command_id == IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH ||
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

    if (command_id == IDC_OMNIBOX_CONTEXT_DEEP_RESEARCH) {
      return omnibox::IsDeepSearchEnabled(profile);
    }
    if (command_id == IDC_OMNIBOX_CONTEXT_CREATE_IMAGES) {
      return omnibox::IsCreateImagesEnabled(profile);
    }
  }

  return true;
}
