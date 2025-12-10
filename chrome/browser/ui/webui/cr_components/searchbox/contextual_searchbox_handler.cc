// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/contextual_input.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/vector_icons.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/url_constants.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#endif

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

// Returns the ContextualSearchSessionHandle for the given WebContents, or
// nullptr if there is none.
contextual_search::ContextualSearchSessionHandle* GetSessionHandle(
    content::WebContents* web_contents) {
  auto* contextual_search_web_contents_helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents);
  return contextual_search_web_contents_helper
             ? contextual_search_web_contents_helper->session_handle()
             : nullptr;
}

}  // namespace

ContextualOmniboxClient::ContextualOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents)
    : SearchboxOmniboxClient(profile, web_contents) {}

ContextualOmniboxClient::~ContextualOmniboxClient() = default;

std::optional<lens::proto::LensOverlaySuggestInputs>
ContextualOmniboxClient::GetLensOverlaySuggestInputs() const {
  auto* contextual_session_handle = GetSessionHandle(web_contents());
  return contextual_session_handle
             ? contextual_session_handle->GetSuggestInputs()
             : std::nullopt;
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
  for (int i = 0; i < tab_strip_model->count(); i++) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    tabs::TabInterface* const tab = tab_strip_model->GetTabAtIndex(i);
    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, i);
    const auto& last_committed_url = tab_renderer_data.last_committed_url;
    // Skip tabs that are still loading, and skip webui.
    const bool is_invalid_url = !last_committed_url.is_valid();
    const bool is_internal_page =
        last_committed_url.SchemeIs(content::kChromeUIScheme) ||
        last_committed_url.SchemeIs(content::kChromeUIUntrustedScheme);

    if (is_invalid_url || is_internal_page) {
      continue;
    }

    auto tab_data = searchbox::mojom::TabInfo::New();
    tab_data->tab_id = tab->GetHandle().raw_value();
    tab_data->title = base::UTF16ToUTF8(tab_renderer_data.title);
    tab_data->url = last_committed_url;
    tab_data->show_in_recent_tab_chip =
        !google_util::IsGoogleSearchUrl(last_committed_url);
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

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->RecordTabContextMenuMetrics(tab_strip_model->count(),
                                                  duplicate_count);
  }

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
    std::unique_ptr<OmniboxController> controller)
    : SearchboxHandler(std::move(pending_searchbox_handler),
                       profile,
                       web_contents,
                       std::move(controller)),
      web_contents_(web_contents) {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  if (contextual_session_handle) {
    if (auto* query_controller = contextual_session_handle->GetController()) {
      file_upload_status_observer_.Observe(query_controller);
    }

    auto* browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents_);
    if (browser_window_interface) {
      browser_window_interface->GetTabStripModel()->AddObserver(this);
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  contextual_tasks_context_service_ =
      contextual_tasks::ContextualTasksContextServiceFactory::GetForProfile(
          profile);
#endif
}

ContextualSearchboxHandler::~ContextualSearchboxHandler() {
  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents_);
  if (helper && helper->session_handle()) {
    auto* browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents_);
    if (browser_window_interface) {
      browser_window_interface->GetTabStripModel()->RemoveObserver(this);
    }
  }
}

contextual_search::ContextualSearchMetricsRecorder*
ContextualSearchboxHandler::GetMetricsRecorder() {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  return contextual_session_handle
             ? contextual_session_handle->GetMetricsRecorder()
             : nullptr;
}

void ContextualSearchboxHandler::NotifySessionStarted() {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  if (contextual_session_handle) {
    contextual_session_handle->NotifySessionStarted();
  }
}

void ContextualSearchboxHandler::NotifySessionAbandoned() {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  if (contextual_session_handle) {
    contextual_session_handle->NotifySessionAbandoned();
  }
}

void ContextualSearchboxHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info_mojom,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  if (contextual_session_handle) {
    context_input_data_ = std::nullopt;
    contextual_session_handle->AddFileContext(
        file_info_mojom->mime_type, std::move(file_bytes),
        CreateImageEncodingOptions(), std::move(callback));
  }
}

void ContextualSearchboxHandler::AddTabContext(int32_t tab_id,
                                               bool delay_upload,
                                               AddTabContextCallback callback) {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  if (!contextual_session_handle) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // TODO(crbug.com/458050417): Move more of the tab context logic to
  // ContextualSessionHandle.
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  RecordTabClickedMetric(tab);

  contextual_session_handle->AddTabContext(
      tab_id,
      base::BindOnce(&ContextualSearchboxHandler::OnAddTabContextTokenCreated,
                     weak_ptr_factory_.GetWeakPtr(), tab_id, delay_upload,
                     std::move(callback)));
}

std::vector<base::UnguessableToken>
ContextualSearchboxHandler::GetUploadedContextTokens() {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  if (contextual_session_handle) {
    return contextual_session_handle->GetUploadedContextTokens();
  }
  return {};
}

void ContextualSearchboxHandler::OnAddTabContextTokenCreated(
    int32_t tab_id,
    bool delay_upload,
    AddTabContextCallback callback,
    const base::UnguessableToken& context_token) {
  // TODO(crbug.com/458050417): Move more of the tab context logic to
  // ContextualSessionHandle.
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  lens::TabContextualizationController* tab_contextualization_controller =
      tab->GetTabFeatures()->tab_contextualization_controller();
  tab_contextualization_controller->GetPageContext(base::BindOnce(
      &ContextualSearchboxHandler::OnGetTabPageContext,
      weak_ptr_factory_.GetWeakPtr(), delay_upload, context_token));
  std::move(callback).Run(context_token);
}

void ContextualSearchboxHandler::RecordTabClickedMetric(
    tabs::TabInterface* const tab) {
  auto* metrics_recorder = GetMetricsRecorder();
  if (!metrics_recorder) {
    return;
  }

  bool has_duplicate_title = false;
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!browser_window_interface) {
    return;
  }

  auto* tab_strip_model = browser_window_interface->GetTabStripModel();
  int tab_index = tab_strip_model->GetIndexOfTab(tab);
  if (tab_index == TabStripModel::kNoTab) {
    return;
  }

  TabRendererData current_tab_renderer_data =
      TabRendererData::FromTabInModel(tab_strip_model, tab_index);
  const std::u16string& current_title = current_tab_renderer_data.title;

  int title_count = 0;
  std::vector<std::pair<int, base::TimeTicks>> last_active_times;
  for (int i = 0; i < tab_strip_model->count(); i++) {
    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, i);
    if (tab_renderer_data.title == current_title) {
      title_count++;
    }

    if (tab_renderer_data.tab_interface) {
      last_active_times.emplace_back(
          i, tab_renderer_data.tab_interface->GetContents()
                 ->GetLastActiveTimeTicks());
    }
  }
  if (title_count > 1) {
    has_duplicate_title = true;
  }

  std::vector<std::pair<int, base::TimeTicks>> reverse_chron_last_active_times(
      last_active_times.begin(), last_active_times.end());
  std::sort(reverse_chron_last_active_times.begin(),
            reverse_chron_last_active_times.end(),
            [](const std::pair<int, base::TimeTicks>& a,
               const std::pair<int, base::TimeTicks>& b) {
              return a.second > b.second;
            });
  std::optional<int> recency_ranking;
  for (size_t i = 0; i < reverse_chron_last_active_times.size(); ++i) {
    if (reverse_chron_last_active_times[i].first == tab_index) {
      recency_ranking = i;
      break;
    }
  }

  metrics_recorder->RecordTabClickedMetrics(has_duplicate_title,
                                            recency_ranking);
}

void ContextualSearchboxHandler::DeleteContext(
    const base::UnguessableToken& context_token) {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  int num_files = 0;
  if (contextual_session_handle) {
    contextual_session_handle->DeleteFile(context_token);
    num_files =
        contextual_session_handle->GetController()->GetFileInfoList().size();
  }

  // If the context token matches the cached tab context, we clear the snapshot.
  if (tab_context_snapshot_.has_value() &&
      tab_context_snapshot_.value().first == context_token) {
    tab_context_snapshot_.reset();
    context_input_data_ = std::nullopt;
  } else if (num_files == 0 && tab_context_snapshot_.has_value()) {
    context_input_data_ = std::optional(*tab_context_snapshot_.value().second);
  }
}

void ContextualSearchboxHandler::ClearFiles() {
  if (auto* contextual_session_handle = GetSessionHandle(web_contents_)) {
    contextual_session_handle->ClearFiles();
  }
  context_input_data_ = std::nullopt;
  tab_context_snapshot_.reset();
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
  // TODO(crbug.com/465427521): This implementation may be able to be removed
  // for now since this is handled in `ComposeboxHandler`.
  omnibox::ChromeAimEntryPoint aim_entry_point =
      PageClassificationToAimEntryPoint(
          omnibox_controller()->client()->GetPageClassification(
              /*is_prefetch=*/false));
  ComputeAndOpenQueryUrl(query_text, disposition, aim_entry_point,
                         /*additional_params=*/{});
}

void ContextualSearchboxHandler::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {
  page_->OnContextualInputStatusChanged(
      file_token, contextual_search::ToMojom(file_upload_status),
      error_type.has_value()
          ? std::make_optional(contextual_search::ToMojom(error_type.value()))
          : std::nullopt);
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
    omnibox::ChromeAimEntryPoint aim_entry_point,
    std::map<std::string, std::string> additional_params) {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  std::vector<const contextual_search::FileInfo*> file_info_list;
  if (contextual_session_handle) {
    // Upload the cached tab context if it exists.
    UploadSnapshotTabContextIfPresent();

    auto search_url_request_info =
        std::make_unique<contextual_search::ContextualSearchContextController::
                             CreateSearchUrlRequestInfo>();
    // This is the time that the user clicked the submit button, however
    // optional autocomplete logic may be run before this if there was a match
    // associated with the query.
    search_url_request_info->query_start_time = base::Time::Now();
    search_url_request_info->query_text = query_text;
    search_url_request_info->additional_params = additional_params;
    search_url_request_info->aim_entry_point = aim_entry_point;

    OpenUrl(contextual_session_handle->CreateSearchUrl(
                std::move(search_url_request_info)),
            disposition);

    file_info_list =
        contextual_session_handle->GetController()->GetFileInfoList();
  }

#if !BUILDFLAG(IS_ANDROID)
  // Assume that if we're here and created a composebox query controller that
  // this is an AIM search by default.
  // Do not provide a callback as this method is only used for dark experiment.
  if (contextual_tasks_context_service_) {
    std::vector<GURL> explicit_urls;
    for (const contextual_search::FileInfo* file_info : file_info_list) {
      if (file_info->tab_url) {
        explicit_urls.push_back(*(file_info->tab_url));
      }
    }
    contextual_tasks_context_service_->GetRelevantTabsForQuery(
        contextual_tasks::TabSelectionOptions(), query_text, explicit_urls,
        base::DoNothing());
  }
#endif
  ClearFiles();
}

void ContextualSearchboxHandler::OnGetTabPageContext(
    bool delay_upload,
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  auto uploaded_context_tokens = GetUploadedContextTokens();
  // Check if the context token is in the list of uploaded context tokens.
  auto it = std::find(uploaded_context_tokens.begin(),
                      uploaded_context_tokens.end(), context_token);
  if (it == uploaded_context_tokens.end()) {
    // Tab was deleted before the file upload flow could start.
    return;
  }

  if (delay_upload) {
    SnapshotTabContext(context_token, std::move(page_content_data));
  } else {
    UploadTabContext(context_token, std::move(page_content_data));
  }
}

void ContextualSearchboxHandler::SnapshotTabContext(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);
  if (contextual_session_handle) {
    context_input_data_ =
        contextual_session_handle->GetController()->GetFileInfoList().size() > 0
            ? std::nullopt
            : std::optional(*page_content_data);
  }
  tab_context_snapshot_.emplace(context_token, std::move(page_content_data));

  page_->OnContextualInputStatusChanged(
      context_token,
      contextual_search::ToMojom(
          contextual_search::FileUploadStatus::kProcessing),
      std::nullopt);
}

void ContextualSearchboxHandler::UploadTabContext(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  auto* contextual_session_handle = GetSessionHandle(web_contents_);

  if (contextual_session_handle) {
    context_input_data_ = std::nullopt;
    contextual_session_handle->StartTabContextUploadFlow(
        context_token, std::move(page_content_data),
        CreateImageEncodingOptions());
  }
}

void ContextualSearchboxHandler::UploadSnapshotTabContextIfPresent() {
  if (!tab_context_snapshot_.has_value()) {
    return;
  }

  auto [context_token, page_content_data] =
      std::move(tab_context_snapshot_.value());
  tab_context_snapshot_.reset();

  UploadTabContext(context_token, std::move(page_content_data));
}

void ContextualSearchboxHandler::OpenUrl(
    GURL url,
    const WindowOpenDisposition disposition) {
  if (OmniboxPopupWebContentsHelper::FromWebContents(web_contents_.get())) {
    auto* browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents_);
    content::OpenURLParams params(url, content::Referrer(), disposition,
                                  ui::PAGE_TRANSITION_LINK, false);
    browser_window_interface->GetTabStripModel()
        ->GetActiveWebContents()
        ->OpenURL(params, base::DoNothing());
  } else {
    content::OpenURLParams params(url, content::Referrer(), disposition,
                                  ui::PAGE_TRANSITION_LINK, false);
    web_contents_->OpenURL(params, base::DoNothing());
  }
}
