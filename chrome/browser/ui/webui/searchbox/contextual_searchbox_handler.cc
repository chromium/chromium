// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/omnibox/contextual_session_web_contents_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/lens/contextual_input.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/composebox/contextual_session_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/url_constants.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"

using composebox::SessionState;

namespace {

constexpr int kThumbnailWidth = 125;
constexpr int kThumbnailHeight = 200;

std::optional<lens::ImageEncodingOptions> CreateImageEncodingOptions() {
  auto image_upload_config =
      ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
  return lens::ImageEncodingOptions{
      .enable_webp_encoding = image_upload_config.enable_webp_encoding(),
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};
}

}  // namespace

ContextualOmniboxClient::ContextualOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents)
    : SearchboxOmniboxClient(profile, web_contents) {}

ContextualOmniboxClient::~ContextualOmniboxClient() = default;

ComposeboxQueryController* ContextualOmniboxClient::GetQueryController() const {
  auto* contextual_session_web_contents_helper =
      ContextualSessionWebContentsHelper::FromWebContents(web_contents());
  auto* contextual_session_handle =
      contextual_session_web_contents_helper
          ? contextual_session_web_contents_helper->session_handle()
          : nullptr;
  return contextual_session_handle ? contextual_session_handle->GetController()
                                   : nullptr;
}

std::optional<lens::proto::LensOverlaySuggestInputs>
ContextualOmniboxClient::GetLensOverlaySuggestInputs() const {
  auto* query_controller = GetQueryController();
  if (!query_controller) {
    return std::nullopt;
  }

  const auto& suggest_inputs = query_controller->suggest_inputs();
  if (suggest_inputs.has_encoded_request_id()) {
    return suggest_inputs;
  }

  return std::nullopt;
}

void ContextualSearchboxHandler::GetRecentTabs(GetRecentTabsCallback callback) {
  std::vector<searchbox::mojom::TabInfoPtr> tabs;

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!browser_window_interface) {
    std::move(callback).Run(std::move(tabs));
    return;
  }

  // Iterate through the tab strip model, getting the data for each tab
  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  UMA_HISTOGRAM_COUNTS_1000(
      "NewTabPage.Composebox.ActiveTabsCountOnContextMenuOpen",
      tab_strip_model->count());

  for (int i = 0; i < tab_strip_model->count(); i++) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    tabs::TabInterface* const tab = tab_strip_model->GetTabAtIndex(i);
    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, i);
    const auto& last_committed_url = tab_renderer_data.last_committed_url;
    // Skip tabs that are still loading, and skip webui.
    if (!last_committed_url.is_valid() || last_committed_url.is_empty() ||
        last_committed_url.SchemeIs(content::kChromeUIScheme) ||
        last_committed_url.SchemeIs(content::kChromeUIUntrustedScheme)) {
      continue;
    }
    auto tab_data = searchbox::mojom::TabInfo::New();
    tab_data->tab_id = tab->GetHandle().raw_value();
    tab_data->title = base::UTF16ToUTF8(tab_renderer_data.title);
    tab_data->url = last_committed_url;
    tab_data->last_active =
        std::max(web_contents->GetLastActiveTimeTicks(),
                 web_contents->GetLastInteractionTimeTicks());
    tabs.push_back(std::move(tab_data));
  }

  // Count duplicate tab titles to record in an UMA histogram.
  // For example, If 2 tabs with title "Wikipedia" and 3 tabs with title
  // "Weather" are open, this histogram will record 2.
  std::map<std::string, int> title_counts;
  for (const auto& tab : tabs) {
    title_counts[tab->title]++;
  }
  int duplicate_count =
      std::count_if(title_counts.begin(), title_counts.end(),
                    [](const std::pair<const std::string, int>& pair) {
                      return pair.second > 1;
                    });
  UMA_HISTOGRAM_COUNTS_100000(
      "NewTabPage.Composebox.DuplicateTabTitlesShownCount", duplicate_count);

  // Sort the tabs by last active time, and truncate to the maximum number of
  // tabs to return.
  int max_tab_suggestions =
      std::min(static_cast<int>(tabs.size()),
               ntp_composebox::kContextMenuMaxTabSuggestions.Get());
  std::partial_sort(tabs.begin(), tabs.begin() + max_tab_suggestions,
                    tabs.end(),
                    [](const searchbox::mojom::TabInfoPtr& a,
                       const searchbox::mojom::TabInfoPtr& b) {
                      return a->last_active > b->last_active;
                    });
  tabs.resize(max_tab_suggestions);

  // Invoke the callback with the results.
  std::move(callback).Run(std::move(tabs));
}

void ContextualSearchboxHandler::GetTabPreview(int32_t tab_id,
                                               GetTabPreviewCallback callback) {
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  lens::TabContextualizationController* tab_context_controller =
      tab->GetTabFeatures()->tab_contextualization_controller();

  content::WebContents* web_contents = tab->GetContents();
  tab_context_controller->CaptureScreenshot(
      CreateTabPreviewEncodingOptions(web_contents),
      base::BindOnce(&ContextualSearchboxHandler::OnPreviewReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextualSearchboxHandler::OnPreviewReceived(
    GetTabPreviewCallback callback,
    const SkBitmap& preview_bitmap) {
  std::move(callback).Run(
      preview_bitmap.isNull()
          ? std::nullopt
          : std::make_optional(webui::GetBitmapDataUrl(preview_bitmap)));
}

void ContextualSearchboxHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // TODO(crbug.com/449196853): We should be using the `tab_strip_api` on the
  // typescript side, but it's not visible to `cr_components`, so we're using
  // `TabStripModelObserver` for now until `tab_strip_api` gets moved out of
  // //chrome. The current implementation is likely brittle, as it's not a
  // supported API for external users.
  if (IsRemoteBound()) {
    page_->OnTabStripChanged();
  }
}

std::optional<lens::ImageEncodingOptions>
ContextualSearchboxHandler::CreateTabPreviewEncodingOptions(
    content::WebContents* web_contents) {
  float scale_factor = 1.0f;
  if (content::RenderWidgetHostView* view =
          web_contents->GetRenderWidgetHostView()) {
    scale_factor = view->GetDeviceScaleFactor();
  }
  const int max_height_pixels =
      static_cast<int>(kThumbnailHeight * scale_factor);
  const int max_width_pixels = static_cast<int>(kThumbnailWidth * scale_factor);
  return lens::ImageEncodingOptions{.max_height = max_height_pixels,
                                    .max_width = max_width_pixels};
}

ContextualSearchboxHandler::ContextualSearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<ComposeboxMetricsRecorder> composebox_metrics_recorder,
    std::unique_ptr<OmniboxController> controller)
    : SearchboxHandler(std::move(pending_searchbox_handler),
                       profile,
                       web_contents,
                       std::move(controller)),
      composebox_metrics_recorder_(std::move(composebox_metrics_recorder)),
      web_contents_(web_contents) {
  if (auto* query_controller = GetQueryController()) {
    file_upload_status_observer_.Observe(query_controller);
  }

  auto* helper =
      ContextualSessionWebContentsHelper::FromWebContents(web_contents_);
  if (helper && helper->session_handle()) {
    auto* browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents_);
    if (browser_window_interface) {
      browser_window_interface->GetTabStripModel()->AddObserver(this);
    }
  }
}

ContextualSearchboxHandler::~ContextualSearchboxHandler() {
  auto* helper =
      ContextualSessionWebContentsHelper::FromWebContents(web_contents_);
  if (helper && helper->session_handle()) {
    auto* browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents_);
    if (browser_window_interface) {
      browser_window_interface->GetTabStripModel()->RemoveObserver(this);
    }
  }
}

ComposeboxQueryController* ContextualSearchboxHandler::GetQueryController() {
  auto* contextual_session_web_contents_helper =
      ContextualSessionWebContentsHelper::FromWebContents(web_contents_);
  auto* contextual_session_handle =
      contextual_session_web_contents_helper
          ? contextual_session_web_contents_helper->session_handle()
          : nullptr;
  return contextual_session_handle ? contextual_session_handle->GetController()
                                   : nullptr;
}

void ContextualSearchboxHandler::NotifySessionStarted() {
  if (auto* query_controller = GetQueryController()) {
    query_controller->NotifySessionStarted();
  }
  composebox_metrics_recorder_->NotifySessionStateChanged(
      SessionState::kSessionStarted);
}

void ContextualSearchboxHandler::NotifySessionAbandoned() {
  if (auto* query_controller = GetQueryController()) {
    query_controller->NotifySessionAbandoned();
  }
  composebox_metrics_recorder_->NotifySessionStateChanged(
      SessionState::kSessionAbandoned);
}

void ContextualSearchboxHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info_mojom,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  auto* query_controller = GetQueryController();
  if (!query_controller) {
    return;
  }
  base::UnguessableToken file_token = base::UnguessableToken::Create();

  std::optional<lens::ImageEncodingOptions> image_options = std::nullopt;
  lens::MimeType mime_type;

  if ((file_info_mojom->mime_type).find("pdf") != std::string::npos) {
    mime_type = lens::MimeType::kPdf;
  } else if ((file_info_mojom->mime_type).find("image") != std::string::npos) {
    mime_type = lens::MimeType::kImage;
    image_options = CreateImageEncodingOptions();
  } else {
    NOTREACHED();
  }

  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->primary_content_type = mime_type;

  base::span<const uint8_t> file_data_span = base::span(file_bytes);
  std::vector<uint8_t> file_data_vector(file_data_span.begin(),
                                        file_data_span.end());
  input_data->context_input->push_back(
      lens::ContextualInput(std::move(file_data_vector), mime_type));

  std::move(callback).Run(file_token);
  composebox_metrics_recorder_->RecordFileSizeMetric(mime_type,
                                                     file_bytes.size());
  query_controller->StartFileUploadFlow(file_token, std::move(input_data),
                                        std::move(image_options));
}

void ContextualSearchboxHandler::AddTabContext(
    int32_t tab_id, AddTabContextCallback callback) {
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  RecordTabClickedMetric(tab);

  lens::TabContextualizationController* tab_contextualization_controller =
      tab->GetTabFeatures()->tab_contextualization_controller();
  auto token = base::UnguessableToken::Create();
  tab_contextualization_controller->GetPageContext(
      base::BindOnce(&ContextualSearchboxHandler::OnGetTabPageContext,
                     weak_ptr_factory_.GetWeakPtr(), token));

  std::move(callback).Run(token);
}

void ContextualSearchboxHandler::RecordTabClickedMetric(
    tabs::TabInterface* const tab) {
  bool has_duplicate_title = false;
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (browser_window_interface) {
    auto* tab_strip_model = browser_window_interface->GetTabStripModel();
    int tab_index = tab_strip_model->GetIndexOfTab(tab);
    if (tab_index != TabStripModel::kNoTab) {
      TabRendererData current_tab_renderer_data =
          TabRendererData::FromTabInModel(tab_strip_model, tab_index);
      const std::u16string& current_title = current_tab_renderer_data.title;

      int title_count = 0;
      for (int i = 0; i < tab_strip_model->count(); i++) {
        TabRendererData tab_renderer_data =
            TabRendererData::FromTabInModel(tab_strip_model, i);
        if (tab_renderer_data.title == current_title) {
          title_count++;
        }
      }
      if (title_count > 1) {
        has_duplicate_title = true;
      }
    }
  }

  UMA_HISTOGRAM_BOOLEAN("NewTabPage.Composebox.TabContextAdded", true);

  UMA_HISTOGRAM_BOOLEAN("NewTabPage.Composebox.TabWithDuplicateTitleClicked",
                        has_duplicate_title);
}

void ContextualSearchboxHandler::DeleteContext(
    const base::UnguessableToken& context_token) {
  // It is possible to receive a call to delete a context before that context
  // has been created in the query controller. We queue all context tokens for
  // deletion at query submission time.
  if (auto* query_controller = GetQueryController()) {
    ComposeboxQueryController::FileInfo* file_info =
        query_controller->GetFileInfo(context_token);

    if (file_info == nullptr) {
      deleted_context_tokens_.insert(context_token);
      return;
    }
    lens::MimeType file_type = file_info ? file_info->mime_type_
                                         : lens::MimeType::kUnknown;
    FileUploadStatus file_status =
        file_info ? file_info->GetFileUploadStatus()
                  : FileUploadStatus::kNotUploaded;

    bool success = query_controller->DeleteFile(context_token);
    composebox_metrics_recorder_->RecordFileDeletedMetrics(
        success, file_type, file_status);
  }
}

void ContextualSearchboxHandler::ClearFiles() {
  if (auto* query_controller = GetQueryController()) {
    query_controller->ClearFiles();
  }
}

void ContextualSearchboxHandler::SubmitQuery(const std::string& query_text,
                                             uint8_t mouse_button,
                                             bool alt_key,
                                             bool ctrl_key,
                                             bool meta_key,
                                             bool shift_key) {
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  ComputeAndOpenQueryUrl(query_text, disposition, /*additional_params=*/{});
}

void ContextualSearchboxHandler::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    composebox_query::mojom::FileUploadStatus file_upload_status,
    const std::optional<FileUploadErrorType>& error_type) {
  page_->OnContextualInputStatusChanged(file_token, file_upload_status,
                                        error_type);
  composebox_metrics_recorder_->OnFileUploadStatusChanged(
      mime_type, file_upload_status, error_type);
}

std::string ContextualSearchboxHandler::AutocompleteIconToResourceName(
    const gfx::VectorIcon& icon) const {
  // The default icon for contextual suggestions is the subdirectory arrow right
  // icon. For the Lens composebox and realbox, we want to stay consistent with
  // the search loupe instead.
  if (icon.name == omnibox::kSubdirectoryArrowRightIcon.name) {
    return searchbox_internal::kSearchIconResourceName;
  }

  return SearchboxHandler::AutocompleteIconToResourceName(icon);
}

void ContextualSearchboxHandler::ComputeAndOpenQueryUrl(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    std::map<std::string, std::string> additional_params) {
  auto* query_controller = GetQueryController();
  if (!query_controller) {
    return;
  }

  // This is the time that the user clicked the submit button, however optional
  // autocomplete logic may be run before this if there was a match associated
  // with the query.
  base::Time query_start_time = base::Time::Now();
  composebox_metrics_recorder_->NotifySessionStateChanged(
      SessionState::kQuerySubmitted);
  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
      search_url_request_info = std::make_unique<
          ComposeboxQueryController::CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = query_text;
  search_url_request_info->query_start_time = query_start_time;
  search_url_request_info->additional_params = additional_params;
  OpenUrl(query_controller->CreateSearchUrl(std::move(search_url_request_info)),
          disposition);
  composebox_metrics_recorder_->NotifySessionStateChanged(
      SessionState::kNavigationOccurred);
  composebox_metrics_recorder_->RecordQueryMetrics(
      query_text.size(), query_controller->num_files_in_request());
}

void ContextualSearchboxHandler::OnGetTabPageContext(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (deleted_context_tokens_.contains(context_token))  {
    // Tab was deleted before the file upload flow could start.
    deleted_context_tokens_.erase(context_token);
    return;
  }
  if (auto* query_controller = GetQueryController()) {
    query_controller->StartFileUploadFlow(context_token,
                                          std::move(page_content_data),
                                          CreateImageEncodingOptions());
  }
}

void ContextualSearchboxHandler::OpenUrl(
    GURL url,
    const WindowOpenDisposition disposition) {
  content::OpenURLParams params(url, content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params, base::DoNothing());
}
