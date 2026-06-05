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
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/feature_engagement/non_iph_promo.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/desktop_query_contextualizer_delegate.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/composebox/contextual_search_mojom_traits.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_host_controller.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_sanitizer.h"
#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_request.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

constexpr int kThumbnailWidth = 125;
constexpr int kThumbnailHeight = 200;
#if !BUILDFLAG(IS_ANDROID)
constexpr size_t kMaxSize = 100 * 1000 * 1000;
#endif

bool IsMimeTypeAllowed(const std::string& mime_type,
                       const std::string& allowed_types_str) {
  std::string lower_mime_type = base::ToLowerASCII(mime_type);
  std::vector<std::string> allowed_types = base::SplitString(
      allowed_types_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& type : allowed_types) {
    if (base::EndsWith(type, "/*", base::CompareCase::SENSITIVE)) {
      std::string prefix = type.substr(0, type.length() - 1);
      if (base::StartsWith(lower_mime_type, prefix,
                           base::CompareCase::SENSITIVE)) {
        return true;
      }
    } else if (lower_mime_type == type) {
      return true;
    }
  }
  return false;
}

omnibox::InputType GetInputType(const std::string& type,
                                const std::string& image_file_types) {
  if (type == "tab") {
    return omnibox::INPUT_TYPE_BROWSER_TAB;
  }
  if (type == "image") {
    return omnibox::INPUT_TYPE_LENS_IMAGE;
  }
  if (type == "pdf") {
    return omnibox::INPUT_TYPE_LENS_FILE;
  }

  if (IsMimeTypeAllowed(type, image_file_types)) {
    return omnibox::INPUT_TYPE_LENS_IMAGE;
  }

  // Arbitrary file types are treated as Lens files.
  return omnibox::INPUT_TYPE_LENS_FILE;
}

content::WebContents* GetActiveTabWebContents(
    content::WebContents* web_contents) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents);
  if (browser_window_interface) {
    if (auto* tab_list = TabListInterface::From(browser_window_interface)) {
      if (auto* active_tab = tab_list->GetActiveTab()) {
        return active_tab->GetContents();
      }
    }
  }
  return web_contents;
}

}  // namespace

// static
std::optional<lens::ImageEncodingOptions>
ContextualSearchboxHandler::CreateImageEncodingOptions() {
  const auto& image_upload_config =
      ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
  return lens::ImageEncodingOptions{
      .enable_webp_encoding = image_upload_config.enable_webp_encoding(),
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};
}

ContextualOmniboxClient::ContextualOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents)
    : SearchboxOmniboxClient(profile, web_contents) {}

ContextualOmniboxClient::~ContextualOmniboxClient() = default;

std::optional<lens::proto::LensOverlaySuggestInputs>
ContextualOmniboxClient::GetLensOverlaySuggestInputs() const {
  return suggest_inputs_callback_ ? suggest_inputs_callback_.Run()
                                  : std::nullopt;
}

int ContextualSearchboxHandler::GetContextMenuMaxTabSuggestions() {
  omnibox::InputState input_state = GetInputState();
  if (auto it = input_state.max_inputs_by_type.find(
          omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
      it != input_state.max_inputs_by_type.end()) {
    return it->second;
  }
  return ntp_composebox::kContextMenuMaxTabSuggestions.Get();
}

void ContextualSearchboxHandler::GetRecentTabs(GetRecentTabsCallback callback) {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!browser_window_interface) {
    std::move(callback).Run({});
    return;
  }

  // Get tabs with recency only first, for the sort and cull step.
  auto* tab_list = TabListInterface::From(browser_window_interface);
  if (!tab_list) {
    std::move(callback).Run({});
    return;
  }
  struct TabTime {
    raw_ptr<tabs::TabInterface> tab;
    base::TimeTicks time;
  };
  std::vector<TabTime> tab_times;
  tabs::TabInterface* active_tab_interface = tab_list->GetActiveTab();
  content::WebContents* active_web_contents =
      active_tab_interface ? active_tab_interface->GetContents() : nullptr;
  for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
    content::WebContents* web_contents = tab->GetContents();
    const GURL& url = web_contents->GetLastCommittedURL();
    if (!url.is_valid()) {
      continue;
    }
    bool is_internal_page = url.SchemeIs(content::kChromeUIScheme) ||
                            url.SchemeIs(content::kChromeUIUntrustedScheme);

    if (!is_internal_page) {
      tab_times.push_back({
          .tab = tab,
          .time = std::max(web_contents->GetLastActiveTimeTicks(),
                           web_contents->GetLastInteractionTimeTicks()),
      });
    }
  }

  // Sort the tabs by last active time.
  auto cmp = [](const TabTime& a, const TabTime& b) { return a.time > b.time; };
  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    std::sort(tab_times.begin(), tab_times.end(), cmp);
  } else {
    int max_tab_suggestions = std::min(static_cast<int>(tab_times.size()),
                                       GetContextMenuMaxTabSuggestions());
    std::partial_sort(tab_times.begin(),
                      tab_times.begin() + max_tab_suggestions, tab_times.end(),
                      cmp);
    tab_times.resize(max_tab_suggestions);
  }

  // Now that tabs have been culled, extract data for only this most recent
  // selection, which is a small subset of all tabs.
  std::vector<searchbox::mojom::TabInfoPtr> tabs;
  for (const TabTime& tab_time : tab_times) {
    content::WebContents* web_contents = tab_time.tab->GetContents();
    const GURL& last_committed_url = web_contents->GetLastCommittedURL();

    auto tab_data = searchbox::mojom::TabInfo::New();
    tab_data->tab_id = tab_time.tab->GetHandle().raw_value();
    tab_data->title = base::UTF16ToUTF8(web_contents->GetTitle());
    tab_data->url = last_committed_url;
    const bool show_in_current_tab_chip =
        active_web_contents &&
        active_web_contents->GetLastCommittedURL() == last_committed_url;
    tab_data->show_in_current_tab_chip = show_in_current_tab_chip;

    lens::TabContextualizationController* tab_context_controller =
        lens::TabContextualizationController::From(tab_time.tab);
    tab_data->show_in_previous_tab_chip =
        !google_util::IsGoogleSearchUrl(last_committed_url) &&
        tab_context_controller->GetInitialPageContextEligibility() &&
        active_web_contents &&
        active_web_contents->GetLastCommittedURL() ==
            chrome::ChromeUINewTabURLAsGURL() &&
        !show_in_current_tab_chip;
    tab_data->last_active = tab_time.time;
    tabs.push_back(std::move(tab_data));
  }

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    // Count duplicate tab titles to record in an UMA histogram.
    // For example, if 2 tabs with title "Wikipedia" and 3 tabs with title
    // "Weather" are open, this histogram will record 2.
    // Note however that since the tab count is limited to 3 tabs in the
    // default configuration, in practice this will not exceed 1 unless
    // the max tab count is increased (4 tabs -> max 2 duplicates, etc.).
    std::map<std::string, int> title_counts;
    for (const auto& tab : tabs) {
      title_counts[tab->title]++;
    }
    const int duplicate_count = std::ranges::count_if(
        title_counts, [](const std::pair<const std::string, int>& pair) {
          return pair.second > 1;
        });

    metrics_recorder->RecordTabContextMenuMetrics(tab_list->GetTabCount(),
                                                  duplicate_count);
  }

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
      lens::TabContextualizationController::From(tab);

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
    mojo::PendingRemote<searchbox::mojom::Page> pending_page,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<OmniboxController> controller,
    GetSessionHandleCallback get_session_callback)
    : SearchboxHandler(std::move(pending_searchbox_handler),
                       std::move(pending_page),
                       profile,
                       web_contents,
                       std::move(controller)),
      get_session_callback_(std::move(get_session_callback)) {
  InitializeInputStateModel();

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (browser_window_interface) {
    if (auto* tab_list = TabListInterface::From(browser_window_interface)) {
      // TODO(crbug.com/449196853): We should be using the `tab_strip_api` on
      // the typescript side, but it's not visible to `cr_components`, so we're
      // using `TabListInterfaceObserver` for now until `tab_strip_api` gets
      // moved out of
      // //chrome. The current implementation is likely brittle, as it's not a
      // supported API for external users.
      tab_list_observation_.Observe(tab_list);
    }
  }

  contextual_tasks_context_service_ =
      contextual_tasks::ContextualTasksContextServiceFactory::GetForProfile(
          profile);
  contextual_tasks_service_ =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile);
  // It is safe to use base::Unretained(this) here because `desktop_delegate_`
  // is owned by `this` and will be destroyed when `this` is destroyed,
  // cancelling any pending callbacks.
  desktop_delegate_ =
      std::make_unique<contextual_tasks::DesktopQueryContextualizerDelegate>(
          base::BindRepeating(
              &ContextualSearchboxHandler::GetContextualSessionHandle,
              base::Unretained(this)),
          base::BindRepeating(
              &ContextualSearchboxHandler::CreateImageEncodingOptions),
          contextual_tasks_context_service_,
          webui::GetBrowserWindowInterface(web_contents_));
  query_contextualizer_ =
      std::make_unique<contextual_tasks::QueryContextualizer>(
          contextual_tasks_service_, desktop_delegate_.get());
}

void ContextualSearchboxHandler::UpdateTabListObservation(
    TabListInterface* tab_list) {
  tab_list_observation_.Reset();
  if (tab_list) {
    tab_list_observation_.Observe(tab_list);
  }
}

void ContextualSearchboxHandler::OnTabAdded(TabListInterface& tab_list,
                                            tabs::TabInterface* tab,
                                            int index) {
  page_->OnTabStripChanged();
}

void ContextualSearchboxHandler::OnActiveTabChanged(TabListInterface& tab_list,
                                                    tabs::TabInterface* tab) {
  page_->OnTabStripChanged();
}

void ContextualSearchboxHandler::OnTabRemoved(TabListInterface& tab_list,
                                              tabs::TabInterface* tab,
                                              TabRemovedReason removed_reason) {
  page_->OnTabStripChanged();
}

void ContextualSearchboxHandler::OnTabListDestroyed(
    TabListInterface& tab_list) {
  tab_list_observation_.Reset();
}

void ContextualSearchboxHandler::OnAllTabsAreClosing(
    TabListInterface& tab_list) {
  page_->OnTabStripChanged();
}

contextual_search::ContextualSearchSessionHandle*
ContextualSearchboxHandler::GetContextualSessionHandle() {
  if (!get_session_callback_) {
    return nullptr;
  }

  auto* session_handle = get_session_callback_.Run();
  auto* context_controller =
      session_handle ? session_handle->GetController() : nullptr;
  // Remove the old context controller if it's different from the new one.
  if (context_controller_ && context_controller_.get() != context_controller) {
    context_controller_->RemoveObserver(this);
    context_controller_ = nullptr;
  }
  // Reset to the new context controller if it is different.
  if (context_controller && !context_controller_) {
    context_controller->AddObserver(this);
    context_controller_ = context_controller->AsWeakPtr();
  }
  return session_handle;
}

ContextualSearchboxHandler::~ContextualSearchboxHandler() {
  query_contextualizer_.reset();
  if (context_controller_) {
    context_controller_->RemoveObserver(this);
  }
  // Ensure any selected tabs are cleared when shutting down.
  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    if (auto* active_task_context_provider = GetActiveTaskContextProvider()) {
      // Clear local tab underlines specific to this surface, not all local tab
      // underlines shared browser-wide.
      for (const auto& [token, handle] : selected_tabs) {
        active_task_context_provider->RemoveLocalTabUnderline(
            tabs::TabHandle(handle));
      }
    }
  }
}

void ContextualSearchboxHandler::ResetInputStateModel() {
  input_state_model_.reset();
}

contextual_search::ContextualSearchMetricsRecorder*
ContextualSearchboxHandler::GetMetricsRecorder() {
  auto* contextual_session_handle = GetContextualSessionHandle();
  return contextual_session_handle
             ? contextual_session_handle->GetMetricsRecorder()
             : nullptr;
}

std::optional<lens::proto::LensOverlaySuggestInputs>
ContextualSearchboxHandler::GetSuggestInputs() {
  auto* contextual_session_handle = GetContextualSessionHandle();
  return contextual_session_handle
             ? contextual_session_handle->GetSuggestInputs()
             : std::nullopt;
}

omnibox::InputState ContextualSearchboxHandler::GetInputState() const {
  if (input_state_model_) {
    return input_state_model_->GetInputState();
  }
  return omnibox::InputState();
}

std::string ContextualSearchboxHandler::GetPreviousQuery() {
  auto* contextual_session_handle = GetContextualSessionHandle();
  return contextual_session_handle &&
                 !contextual_session_handle->previous_turns().empty()
             ? contextual_session_handle->previous_turns().back().query
             : std::string();
}

bool ContextualSearchboxHandler::IsSmartTabSharingActive() const {
  if (smart_tab_sharing_active_for_thread_.has_value()) {
    return *smart_tab_sharing_active_for_thread_;
  }
  if (profile_) {
    return profile_->GetPrefs()->GetBoolean(
        contextual_tasks::kContextualTasksShareOpenTabsEveryThread);
  }
  return false;
}

void ContextualSearchboxHandler::SetSmartTabSharingActive(bool active) {
  if (!contextual_tasks::ContextualTasksContextService::
          GetIsSmartTabSharingEnabled(profile_)) {
    return;
  }
  smart_tab_sharing_active_for_thread_ = active;

#if !BUILDFLAG(IS_ANDROID)
  if (active && profile_ && !has_incremented_sts_activation_count_) {
    has_incremented_sts_activation_count_ = true;
    auto* tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
    if (tracker) {
      tracker->NotifyEvent("smart_tab_sharing_activated");

      // Don't process the default-on promo if STS is already default-on.
      const bool default_on = profile_->GetPrefs()->GetBoolean(
          contextual_tasks::kContextualTasksShareOpenTabsEveryThread);
      if (!default_on &&
          base::FeatureList::IsEnabled(
              contextual_tasks::
                  kContextualTasksContextSmartTabSharingDefaultOnAvailability)) {
        if (feature_engagement::NonIphPromo::RequestPermissionToShow(
                profile_,
                feature_engagement::kIPHSmartTabSharingDefaultOnFeature)) {
          if (auto* web_ui_interface =
                  contextual_tasks::GetWebUiInterface(web_contents_)) {
            if (web_ui_interface->GetPageRemote().is_bound()) {
              web_ui_interface->GetPageRemote()
                  ->ShowSmartTabSharingDefaultOnIph();
            }
          }
        }
      }
    }
  }
#endif
}

void ContextualSearchboxHandler::GetSmartTabSharingActive(
    composebox::mojom::PageHandler::GetSmartTabSharingActiveCallback callback) {
  std::move(callback).Run(IsSmartTabSharingActive());
}

std::vector<int32_t> ContextualSearchboxHandler::GetSelectedTabIds() const {
  std::vector<int32_t> ids;
  for (const auto& entry : selected_tabs) {
    ids.push_back(entry.second);
  }
  return ids;
}

void ContextualSearchboxHandler::NotifySessionStarted() {
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (contextual_session_handle) {
    contextual_session_handle->NotifySessionStarted();
  }
}

void ContextualSearchboxHandler::NotifySessionAbandoned() {
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (contextual_session_handle) {
    contextual_session_handle->NotifySessionAbandoned();
  }
}

void ContextualSearchboxHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info_mojom,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  if (!contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile_->GetPrefs())) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  auto* contextual_session_handle = GetContextualSessionHandle();
  if (!contextual_session_handle) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  context_input_data_ = std::nullopt;
  auto context_token = contextual_session_handle->CreateContextToken();
  // Return the token early, so that listeners can immediately begin
  // listening for file upload updates.
  // TODO(crbug.com/477324337): Consider calling this callback elsewhere in
  // the flow.
  std::move(callback).Run(base::ok(context_token));
  contextual_session_handle->StartFileContextUploadFlow(
      context_token, file_info_mojom->file_name, file_info_mojom->mime_type,
      std::move(file_bytes), CreateImageEncodingOptions());
}

// LINT.IfChange(ContextualSearchboxHandler_AddFileContextFromBrowser)
void ContextualSearchboxHandler::AddFileContextFromBrowser(
    std::string file_name,
    std::string mime_type,
    mojo_base::BigBuffer file_bytes,
    std::optional<lens::ImageEncodingOptions> image_encoding_options,
    AddFileContextCallback callback) {
  if (!contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile_->GetPrefs())) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  auto* contextual_session_handle = GetContextualSessionHandle();
  if (!contextual_session_handle) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  auto composebox_config =
      ntp_composebox::FeatureConfig::Get().config.composebox();
  const std::string image_mime_types =
      composebox_config.image_upload().mime_types_allowed();
  const std::string attachment_mime_types =
      composebox_config.attachment_upload().mime_types_allowed();

  omnibox::InputType input_type = GetInputType(mime_type, image_mime_types);

  omnibox::InputState input_state = GetInputState();

  if (input_state.active_tool == omnibox::TOOL_MODE_DEEP_SEARCH) {
    std::move(callback).Run(
        base::unexpected(contextual_search::ContextUploadErrorType::
                             kBrowserProcessingFileUploadNotAllowedError));
    return;
  } else if (input_state.active_tool != omnibox::TOOL_MODE_UNSPECIFIED) {
    auto& disabled = input_state.disabled_input_types;
    if (std::find(disabled.begin(), disabled.end(), input_type) !=
        disabled.end()) {
      std::move(callback).Run(
          base::unexpected(contextual_search::ContextUploadErrorType::
                               kBrowserProcessingUnsupportedFileTypeError));
      return;
    }
  }

  uint64_t file_size = file_bytes.size();
  uint64_t max_file_size =
      composebox_config.attachment_upload().max_size_bytes();
  if (file_size == 0 || file_size > max_file_size) {
    auto error_type = file_size == 0
                          ? contextual_search::ContextUploadErrorType::
                                kBrowserProcessingFileEmptyError
                          : contextual_search::ContextUploadErrorType::
                                kBrowserProcessingFileTooLargeError;
    std::move(callback).Run(base::unexpected(error_type));
    return;
  }

  bool is_file_allowed = lens::features::IsLensSendRawFileMediaTypesEnabled() ||
                         IsMimeTypeAllowed(mime_type, image_mime_types) ||
                         IsMimeTypeAllowed(mime_type, attachment_mime_types);
  if (!is_file_allowed) {
    std::move(callback).Run(
        base::unexpected(contextual_search::ContextUploadErrorType::
                             kBrowserProcessingUnsupportedFileTypeError));
    return;
  }

  int total_count = 0;
  int current_type_count = 0;
  auto uploaded_files =
      contextual_session_handle->GetUploadedContextFileInfos();
  total_count = uploaded_files.size();
  for (const auto& file : uploaded_files) {
    if (GetInputType(file.mime_type_string.value_or(""), image_mime_types) ==
        input_type) {
      current_type_count++;
    }
  }

  int max_total = composebox_config.max_num_files();
  if (input_state.max_total_inputs > 0) {
    max_total = input_state.max_total_inputs;
  }

  int max_type = max_total;
  if (input_state.max_inputs_by_type.find(input_type) !=
      input_state.max_inputs_by_type.end()) {
    max_type = input_state.max_inputs_by_type.at(input_type);
  }

  if (current_type_count >= max_type) {
    contextual_search::ContextUploadErrorType error_type;
    switch (input_type) {
      case omnibox::INPUT_TYPE_LENS_IMAGE:
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingMaxImagesExceededError;
        break;
      case omnibox::INPUT_TYPE_LENS_FILE:
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingMaxPdfsExceededError;
        break;
      default:
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingMaxFilesExceededError;
        break;
    }
    std::move(callback).Run(base::unexpected(error_type));
    return;
  }

  if (total_count >= max_total) {
    std::move(callback).Run(
        base::unexpected(contextual_search::ContextUploadErrorType::
                             kBrowserProcessingMaxFilesExceededError));
    return;
  }

  auto context_token = contextual_session_handle->CreateContextToken();
  // Return the token early, so that listeners can immediately begin
  // listening for file upload updates.
  // TODO(crbug.com/477324337): Consider calling this callback elsewhere in
  // the flow.
  std::move(callback).Run(base::ok(context_token));
  contextual_session_handle->StartFileContextUploadFlow(
      context_token, file_name, mime_type, std::move(file_bytes),
      std::move(image_encoding_options));
}
// LINT.ThenChange(//ui/webui/resources/cr_components/composebox/composebox_mixin.ts:getValidationError)

void ContextualSearchboxHandler::ContinueAddTabContext(
    int32_t tab_id,
    bool delay_upload,
    base::UnguessableToken context_token,
    AddTabContextCallback callback) {
  // TODO(crbug.com/458050417): Move more of the tab context logic to
  // ContextualSessionHandle.
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  RecordTabAddedMetric(tab, /*is_tab_suggestion_chip=*/delay_upload);

  // Track explicitly selected tabs so their tab underlines can be cleaned up
  // later if needed, and so they can be submitted to cobrowsing from an AIM
  // entrypoint.
  selected_tabs[context_token] = tab_id;

  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    // Underline the tabstrip immediately when the tab context is selected
    // (without submission).
    if (auto* active_task_context_provider = GetActiveTaskContextProvider()) {
      active_task_context_provider->AddLocalTabUnderline(
          tabs::TabHandle(tab_id));
    }
  }

  lens::TabContextualizationController* tab_contextualization_controller =
      lens::TabContextualizationController::From(tab);
  tab_contextualization_controller->GetPageContext(base::BindOnce(
      &ContextualSearchboxHandler::OnGetTabPageContext,
      weak_ptr_factory_.GetWeakPtr(), delay_upload, context_token));

  std::move(callback).Run(base::ok(context_token));
}

void ContextualSearchboxHandler::AddTabContext(int32_t tab_id,
                                               bool delay_upload,
                                               AddTabContextCallback callback) {
  if (!contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile_->GetPrefs())) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (!contextual_session_handle) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }
  auto context_token = contextual_session_handle->CreateContextToken();

  ContinueAddTabContext(tab_id, delay_upload, context_token,
                        std::move(callback));
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualSearchboxHandler::OnDrivePickerDisconnected() {
  if (!drive_upload_click_callback_.is_null()) {
    std::move(drive_upload_click_callback_)
        .Run(searchbox::mojom::DriveUploadResponse::New());
  }
  CleanupDrivePicker();
}

void ContextualSearchboxHandler::OnSelection(
    std::vector<drive_picker_host::mojom::DriveFilePtr> files) {
  if (drive_upload_click_callback_.is_null()) {
    LOG(WARNING) << "No drive upload click callback, aborting upload.";
    CleanupDrivePicker();
    return;
  }

  auto response = searchbox::mojom::DriveUploadResponse::New();

  auto* contextual_session_handle = GetContextualSessionHandle();
  size_t valid_files_count =
      contextual_session_handle
          ? contextual_session_handle->GetUploadedContextFileInfos().size()
          : 0;

  bool count_limit_hit = false;
  bool size_limit_hit = false;
  const size_t max_total_inputs =
      static_cast<size_t>(GetInputState().max_total_inputs);
  if (max_total_inputs == 0) {
    std::move(drive_upload_click_callback_).Run(std::move(response));
    CleanupDrivePicker();
    return;
  }
  const size_t max_files = max_total_inputs;

  for (const auto& file_ptr : files) {
    std::optional<SanitizedDriveFileData> sanitized =
        DrivePickerSanitizer::Sanitize(file_ptr);
    if (!sanitized) {
      drive_picker_result_handler_receiver_.ReportBadMessage(
          "Invalid Drive file data received from renderer.");
      if (!drive_upload_click_callback_.is_null()) {
        std::move(drive_upload_click_callback_)
            .Run(searchbox::mojom::DriveUploadResponse::New());
      }
      CleanupDrivePicker();
      return;
    }
    const auto& file = sanitized.value();

    if (valid_files_count >= max_files) {
      count_limit_hit = true;
      break;
    }

    if (file.size_bytes > kMaxSize) {
      size_limit_hit = true;
      continue;
    }

    if (!contextual_session_handle) {
      continue;
    }

    base::UnguessableToken token =
        contextual_session_handle->CreateContextToken();
    contextual_search::ContextualSearchSessionHandle::DriveUploadParams params;
    params.drive_id = file.drive_id;
    params.resource_key = file.resource_key;
    params.mime_type = file.mime_type;
    params.file_name = file.file_name;
    contextual_session_handle->StartDriveContextUploadFlow(token, params);

    auto success_file = searchbox::mojom::DriveFile::New();
    success_file->token = token;
    success_file->file_name = file.file_name;
    success_file->mime_type = file.mime_type;
    success_file->thumbnail_url =
        file.thumbnail_url ? file.thumbnail_url->spec() : "";
    success_file->icon_url = file.icon_url;

    response->files.push_back(std::move(success_file));
    valid_files_count++;
  }

  // Determine highest priority error. File limit takes precedence over size
  // limit.
  if (count_limit_hit) {
    response->error = searchbox::mojom::DriveUploadError::kMaxFilesExceeded;
  } else if (size_limit_hit) {
    response->error = searchbox::mojom::DriveUploadError::kSizeLimitExceeded;
  }

  std::move(drive_upload_click_callback_).Run(std::move(response));
  CleanupDrivePicker();
}

void ContextualSearchboxHandler::OnCancel() {
  if (!drive_upload_click_callback_.is_null()) {
    std::move(drive_upload_click_callback_)
        .Run(searchbox::mojom::DriveUploadResponse::New());
  }
  CleanupDrivePicker();
}

void ContextualSearchboxHandler::OnError(
    drive_picker_host::mojom::DrivePickerError error) {
  LOG(WARNING) << "Drive picker error: " << error;
  if (!drive_upload_click_callback_.is_null()) {
    std::move(drive_upload_click_callback_)
        .Run(searchbox::mojom::DriveUploadResponse::New());
  }
  CleanupDrivePicker();
}
#endif

void ContextualSearchboxHandler::CleanupDrivePicker() {
#if !BUILDFLAG(IS_ANDROID)
  drive_picker_result_handler_receiver_.reset();
  drive_picker_controller_.reset();
#endif
}

void ContextualSearchboxHandler::OnDriveUploadClicked(
    OnDriveUploadClickedCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(searchbox::mojom::DriveUploadResponse::New());
#else
  CHECK(
      base::FeatureList::IsEnabled(omnibox::kComposeboxDriveContextMenuOption));
  CHECK(contextual_search::ContextualSearchService::IsContextSharingEnabled(
      profile_->GetPrefs()));

  if (!drive_upload_click_callback_.is_null()) {
    return;
  }
  drive_upload_click_callback_ = std::move(callback);

  if (drive_picker_result_handler_receiver_.is_bound()) {
    return;
  }

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!browser_window_interface) {
    return;
  }

  if (!drive_picker_controller_) {
    drive_picker_controller_ =
        std::make_unique<DrivePickerHostController>(browser_window_interface);
  }

  drive_picker_result_handler_receiver_.reset();

  auto request = std::make_unique<drive_picker_host::DrivePickerHostRequest>(
      drive_picker_host::DrivePickerHostRequest::RequestType::kPickerUi,
      drive_picker_result_handler_receiver_.BindNewPipeAndPassRemote());

  drive_picker_controller_->ShowDrivePickerHost(std::move(request));

  drive_picker_result_handler_receiver_.set_disconnect_handler(
      base::BindOnce(&ContextualSearchboxHandler::OnDrivePickerDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
#endif
}

std::vector<base::UnguessableToken>
ContextualSearchboxHandler::GetUploadedContextTokens() {
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (contextual_session_handle) {
    return contextual_session_handle->GetUploadedContextTokens();
  }
  return {};
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

void ContextualSearchboxHandler::SetActiveToolMode(omnibox::ToolMode tool) {
  if (!input_state_model_) {
    return;
  }
  input_state_model_->setActiveTool(tool);
}

void ContextualSearchboxHandler::RecordToolSelectionAction(
    omnibox::ToolMode tool) {
  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->RecordToolMode(tool);
  }
}

void ContextualSearchboxHandler::RecordModelSelectionAction(
    omnibox::ModelMode model) {
  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->RecordModelMode(model);
  }
}

void ContextualSearchboxHandler::SetActiveModelMode(omnibox::ModelMode model) {
  if (!input_state_model_) {
    return;
  }
  input_state_model_->setActiveModel(model);
}

void ContextualSearchboxHandler::ActivateMetricsFunnel(
    const std::string& funnel_name) {
  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->ActivateMetricsFunnel(funnel_name);
  }
}

void ContextualSearchboxHandler::GetInputState(GetInputStateCallback callback) {
  if (!input_state_model_) {
    InitializeInputStateModel();
  }
  if (input_state_model_) {
    std::move(callback).Run(input_state_model_->GetInputState());
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void ContextualSearchboxHandler::OnInputStateChanged(
    const contextual_search::InputState& state) {
  page_->OnInputStateChanged(state);
}

base::WeakPtr<contextual_search::InputStateModel>
ContextualSearchboxHandler::GetOrCreateInputStateModel() {
  auto* session_handle = GetContextualSessionHandle();
  if (!session_handle) {
    return nullptr;
  }

  // In the case of the Omnibox popup, `web_contents_` refers to the popup's own
  // WebContents, which is shared across multiple tabs. To achieve true per-tab
  // persistence, we must associate the `InputStateModel` with the active tab's
  // `WebContents` instead. For other embedders of `ComposeboxHandler` (e.g.,
  // the Side Panel), `web_contents_` already points to the tab's WebContents,
  // so this fallback ensures correct behavior across different embedding
  // contexts.
  content::WebContents* target_web_contents =
      GetActiveTabWebContents(web_contents_);
  auto* user_data =
      contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
          target_web_contents);
  if (!user_data) {
    contextual_tasks::ContextualTasksWebContentsUserData::CreateForWebContents(
        target_web_contents);
    user_data =
        contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
            target_web_contents);
  }

  return user_data->GetOrCreateInputStateModel(*session_handle);
}

void ContextualSearchboxHandler::InitializeInputStateModel() {
  input_state_model_ = GetOrCreateInputStateModel();
  if (!input_state_model_) {
    return;
  }

  if (profile_) {
    input_state_model_->SetPrefService(profile_->GetPrefs());
  }

  input_state_subscription_ = input_state_model_->subscribe(
      base::BindRepeating(&ContextualSearchboxHandler::OnInputStateChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  input_state_model_->Initialize();
}

void ContextualSearchboxHandler::RecordTabAddedMetric(
    tabs::TabInterface* const tab,
    bool is_tab_suggestion_chip) {
// TODO(b/502297163): Implement for Android.
#if !BUILDFLAG(IS_ANDROID)
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

  auto* tab_list = TabListInterface::From(browser_window_interface);
  if (!tab_list) {
    return;
  }
  int tab_index = tab_list->GetIndexOfTab(tab->GetHandle());
  if (tab_index == TabStripModel::kNoTab) {
    return;
  }

  const std::u16string& current_title = tab->GetContents()->GetTitle();

  int title_count = 0;
  std::vector<std::pair<size_t, base::TimeTicks>> last_active_times;
  auto all_tabs = tab_list->GetAllTabs();
  for (size_t i = 0; i < all_tabs.size(); i++) {
    tabs::TabInterface* tab_interface = all_tabs[i];

    const std::u16string& tab_title = tab_interface->GetContents()->GetTitle();
    if (tab_title == current_title) {
      title_count++;
    }

    last_active_times.emplace_back(
        i, tab_interface->GetContents()->GetLastActiveTimeTicks());
  }

  if (title_count > 1) {
    has_duplicate_title = true;
  }

  std::vector<std::pair<size_t, base::TimeTicks>>
      reverse_chron_last_active_times(last_active_times.begin(),
                                      last_active_times.end());
  std::sort(reverse_chron_last_active_times.begin(),
            reverse_chron_last_active_times.end(),
            [](const std::pair<size_t, base::TimeTicks>& a,
               const std::pair<size_t, base::TimeTicks>& b) {
              return a.second > b.second;
            });
  std::optional<int> recency_ranking;
  for (size_t i = 0; i < reverse_chron_last_active_times.size(); ++i) {
    if (reverse_chron_last_active_times[i].first ==
        static_cast<size_t>(tab_index)) {
      recency_ranking = static_cast<int>(i);
      break;
    }
  }

  metrics_recorder->RecordTabAddedMetrics(has_duplicate_title, recency_ranking,
                                          is_tab_suggestion_chip);
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_ANDROID)
bool ContextualSearchboxHandler::ShouldOpenInLensSidePanel(
    content::WebContents* active_web_contents,
    contextual_search::ContextualSearchSessionHandle* session_handle) {
  // In order to open in the lens side panel the following must be
  // true:
  // 1) User is not eligible for contextual tasks
  // 2) Lens M3 is enabled
  // 3) There is only one submitted context token
  // 4) The submitted context token is the active tab
  // 5) Lens Overlay is enabled.
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  auto* eligibility_manager =
      browser_window_interface
          ? contextual_tasks::EntryPointEligibilityManager::From(
                browser_window_interface)
          : nullptr;

  auto* entry_point_controller =
      lens::LensOverlayEntryPointController::From(browser_window_interface);

  return active_web_contents &&
         (!eligibility_manager ||
          !eligibility_manager->AreEntryPointsEligible()) &&
         entry_point_controller && entry_point_controller->IsEnabled() &&
         lens::IsAimM3Enabled(profile_) &&
         session_handle->GetSubmittedContextTokens().size() == 1 &&
         session_handle->IsTabInContext(
             sessions::SessionTabHelper::IdForTab(active_web_contents));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ContextualSearchboxHandler::DeleteContext(
    const base::UnguessableToken& context_token,
    bool from_automatic_chip) {
  // Delete tab underline if it exists:
  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    auto it = selected_tabs.find(context_token);
    if (it != selected_tabs.end()) {
      if (auto* active_task_context_provider = GetActiveTaskContextProvider()) {
        active_task_context_provider->RemoveLocalTabUnderline(
            tabs::TabHandle(it->second));
      }
    }
  }
  selected_tabs.erase(context_token);

  auto* contextual_session_handle = GetContextualSessionHandle();
  int num_files = 0;
  if (contextual_session_handle) {
    contextual_session_handle->DeleteFile(context_token);
    num_files = contextual_session_handle->GetUploadedContextFileInfos().size();
  }

  // If the context token matches the cached tab context, we clear the snapshot.
  if (tab_context_snapshot_.has_value() &&
      tab_context_snapshot_.value().first == context_token) {
    tab_context_snapshot_.reset();
    context_input_data_ = std::nullopt;
  } else if (num_files == 0 && tab_context_snapshot_.has_value()) {
    context_input_data_ = std::optional(*tab_context_snapshot_.value().second);
  }

  // Ensure `input_state_model_` is updated when context deleted.
  if (input_state_model_) {
    input_state_model_->OnContextChanged();
  }
}

void ContextualSearchboxHandler::DeleteContextFromBrowser(
    const base::UnguessableToken& context_token,
    bool from_automatic_chip) {
  DeleteContext(context_token, from_automatic_chip);
  page_->OnContextualInputStatusChanged(
      context_token, contextual_search::ContextUploadStatus::kUploadReplaced,
      std::nullopt);
}

void ContextualSearchboxHandler::ClearFiles(
    bool should_block_auto_suggested_tabs) {
  ClearFiles(should_block_auto_suggested_tabs, /*query_submitted=*/false);
}

void ContextualSearchboxHandler::ClearFiles(
    bool should_block_auto_suggested_tabs,
    bool query_submitted) {
  if (auto* contextual_session_handle = GetContextualSessionHandle()) {
    // Clears files if `query_submitted`=true, and if
    // `omnibox::kContextManagementInComposebox` is enabled.
    contextual_session_handle->ClearFiles(
        query_submitted);
    // Clear cached tab images (snapshots) if tab context no longer exists
    // due to being deleted in `clearFiles` (this will clear it fully
    // if it does decide to).
    if (contextual_session_handle->GetUploadedContextTokens().empty()) {
      context_input_data_ = std::nullopt;
      tab_context_snapshot_.reset();
    }
  } else {  // No active contextual session -> clear all snapshot images.
    context_input_data_ = std::nullopt;
    tab_context_snapshot_.reset();
  }

  // Clear all tab underlines related to only this surface:
  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    if (auto* active_task_context_provider = GetActiveTaskContextProvider()) {
      for (const auto& [token, handle] : selected_tabs) {
        active_task_context_provider->RemoveLocalTabUnderline(
            tabs::TabHandle(handle));
      }
    }
  }

  // Clear token-to-tab id pairs if this function is due to the
  // 'clear all' button being clicked.
  if (!query_submitted) {
    selected_tabs.clear();
  }

  // Ensure `input_state_model_` is updated when context is cleared.
  if (input_state_model_) {
    input_state_model_->OnContextChanged();
  }
}

void ContextualSearchboxHandler::OpenAutocompleteMatch(uint8_t line,
                                                       const GURL& url,
                                                       bool are_matches_showing,
                                                       uint8_t mouse_button,
                                                       bool alt_key,
                                                       bool ctrl_key,
                                                       bool meta_key,
                                                       bool shift_key) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);

  // Record match navigations for composebox matches.
  bool is_zero_suggest = autocomplete_controller()->input().IsZeroSuggest();
  auto* recorder = GetMetricsRecorder();
  bool record_composebox_metric =
      omnibox::IsComposebox(
          omnibox_controller()->client()->GetPageClassification(
              /*is_prefetch=*/false)) &&
      match && recorder;

  if (record_composebox_metric) {
    if (is_zero_suggest) {
      recorder->RecordZeroSuggestClick(match->IsContextualSearchSuggestion());
    } else {
      recorder->RecordTypedSuggestNavigation(match->IsVerbatimType());
    }
  }

  if (recorder) {
    recorder->RecordNoAcMatchSubmitQuery(/*text_length=*/0, /*file_count=*/0,
                                         /*is_ac_match=*/true);
  }

  SearchboxHandler::OpenAutocompleteMatch(line, url, are_matches_showing,
                                          mouse_button, alt_key, ctrl_key,
                                          meta_key, shift_key);
}

void ContextualSearchboxHandler::SetSmartComposeStats(
    searchbox::mojom::SmartComposeStatsPtr smart_compose_stats) {
  if (smart_compose_stats) {
    omnibox::metrics::SmartComposeStats stats;
    stats.set_enabled(smart_compose_stats->enabled);
    stats.set_shown_count(smart_compose_stats->shown_count);
    stats.set_accepted_count(smart_compose_stats->accepted_count);
    stats.set_characters_accepted(smart_compose_stats->characters_accepted);
    stats.set_shown_length(smart_compose_stats->shown_length);
    autocomplete_controller()->SetSmartComposeStats(stats);
  }
}

void ContextualSearchboxHandler::ShouldShowDriveDisclaimer(
    ShouldShowDriveDisclaimerCallback callback) {
  if (!base::FeatureList::IsEnabled(
          omnibox::kComposeboxDriveContextMenuOption)) {
    std::move(callback).Run(false);
    return;
  }
  bool accepted = profile_->GetPrefs()->GetBoolean(
      contextual_search::kDriveDisclaimerAccepted);
  std::move(callback).Run(!accepted);
}

void ContextualSearchboxHandler::OnDriveDisclaimerAccepted() {
  profile_->GetPrefs()->SetBoolean(contextual_search::kDriveDisclaimerAccepted,
                                   true);
}

void ContextualSearchboxHandler::QueryAutocomplete(
    const std::u16string& input,
    bool prevent_inline_autocomplete,
    uint32_t cursor_position) {
  if (contextual_tasks_context_service_) {
    contextual_tasks_context_service_->OnTypedQuery();
  }

  SearchboxHandler::QueryAutocomplete(input, prevent_inline_autocomplete,
                                      cursor_position);
}

void ContextualSearchboxHandler::OnContextUploadStatusChanged(
    const base::UnguessableToken& context_token,
    lens::MimeType mime_type,
    contextual_search::ContextUploadStatus context_upload_status,
    const std::optional<contextual_search::ContextUploadErrorType>&
        error_type) {
  page_->OnContextualInputStatusChanged(context_token, context_upload_status,
                                        error_type);

  // Ensure `input_state_model_` is updated when file is uploaded.
  if (input_state_model_) {
    input_state_model_->OnContextChanged();
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
  // TODO(crbug.com/465427521): This implementation may be able to be removed
  // for now since this is handled in `ComposeboxHandler`.
  omnibox::ChromeAimEntryPoint aim_entry_point =
      PageClassificationToAimEntryPoint(
          omnibox_controller()->client()->GetPageClassification(
              /*is_prefetch=*/false));

  ContextualizeQueryAndOpenUrl(query_text, disposition, aim_entry_point,
                               /*additional_params=*/{});
}

void ContextualSearchboxHandler::MaybeTriggerSmartTabSharingPromo(
    const std::string& query,
    content::WebContents* web_contents_for_window) {
  if (!contextual_tasks_context_service_) {
    return;
  }

  std::vector<GURL> explicit_urls;
  if (auto* contextual_session_handle = GetContextualSessionHandle()) {
    for (const contextual_search::FileInfo* file_info :
         contextual_session_handle->GetController()->GetFileInfoList()) {
      if (file_info->tab_url) {
        explicit_urls.push_back(*(file_info->tab_url));
      }
    }
  }

  contextual_tasks::ConversationThread conversation_thread;
  conversation_thread.query = query;
  if (auto* contextual_session_handle = GetContextualSessionHandle()) {
    conversation_thread.previous_turns =
        contextual_session_handle->previous_turns();
    conversation_thread.shared_tab_titles =
        contextual_session_handle->GetSubmittedContextTabTitles();
  }

  const bool is_eligible_for_promo =
      !IsSmartTabSharingActive() &&
      contextual_tasks::ContextualTasksContextService::
          GetIsSmartTabSharingEnabled(profile_);
  if (is_eligible_for_promo) {
    contextual_tasks::TabSelectionOptions tab_selection_options;
    tab_selection_options.tab_selection_timeout =
        contextual_tasks::GetSmartTabSharingTabSelectionTimeout();
    if (auto* browser_window_interface =
            webui::GetBrowserWindowInterface(web_contents_for_window)) {
      tab_selection_options.browser_window_interface =
          browser_window_interface->GetWeakPtr();
    }
    tab_selection_options.min_model_score = static_cast<float>(
        contextual_tasks::GetSmartTabSharingPromoScoreThreshold());
    contextual_tasks_context_service_->GetRelevantTabsForConversationThread(
        tab_selection_options, conversation_thread, explicit_urls,
        base::BindOnce(
            &ContextualSearchboxHandler::OnRelevantTabsReceivedToMaybeShowPromo,
            weak_ptr_factory_.GetWeakPtr()));
  } else if (!contextual_tasks::ContextualTasksContextService::
                 GetIsSmartTabSharingEnabled(profile_)) {
    // Run dark experiment if smart tab sharing is not enabled and do not
    // block.
    contextual_tasks::TabSelectionOptions tab_selection_options;
    if (auto* browser_window_interface =
            webui::GetBrowserWindowInterface(web_contents_for_window)) {
      tab_selection_options.browser_window_interface =
          browser_window_interface->GetWeakPtr();
    }
    contextual_tasks_context_service_->GetRelevantTabsForConversationThread(
        tab_selection_options, conversation_thread, explicit_urls,
        base::DoNothing());
  }
}

void ContextualSearchboxHandler::ContextualizeQueryAndOpenUrl(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    omnibox::ChromeAimEntryPoint aim_entry_point,
    std::map<std::string, std::string> additional_params) {
  MaybeTriggerSmartTabSharingPromo(query_text, web_contents_);

  if (query_contextualizer_) {
    query_contextualizer_->Contextualize(
        GetTaskId(), query_text, /*tabs_to_recontextualize=*/{},
        /*tabs_to_force_contextualize=*/{},
        /*on_ineligible_callback=*/base::DoNothing(),
        /*on_processed_callback=*/base::DoNothing(),
        base::BindOnce(
            [](ContextualSearchboxHandler* self, const std::string& query,
               WindowOpenDisposition disp,
               omnibox::ChromeAimEntryPoint entry_point,
               std::map<std::string, std::string> params,
               base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
                   handle) {
              self->ComputeAndOpenQueryUrl(query, disp, entry_point,
                                           std::move(params));
            },
            base::Unretained(this), query_text, disposition, aim_entry_point,
            std::move(additional_params)),
        /*enable_smart_tab_selection=*/IsSmartTabSharingActive());
    return;
  }

  ComputeAndOpenQueryUrl(query_text, disposition, aim_entry_point,
                         std::move(additional_params));
}

void ContextualSearchboxHandler::OnRelevantTabsReceivedToMaybeShowPromo(
    std::vector<base::WeakPtr<content::WebContents>> relevant_tabs) {
  if (relevant_tabs.empty()) {
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  if (feature_engagement::NonIphPromo::RequestPermissionToShow(
          profile_, feature_engagement::kIPHSmartTabSharingTryItFeature)) {
    if (auto* web_ui_interface =
            contextual_tasks::GetWebUiInterface(web_contents_)) {
      if (web_ui_interface->GetPageRemote().is_bound()) {
        web_ui_interface->GetPageRemote()->ShowSmartTabSharingTryItIph();
      }
    }
  }
#endif
}

void ContextualSearchboxHandler::ComputeAndOpenQueryUrl(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    omnibox::ChromeAimEntryPoint aim_entry_point,
    std::map<std::string, std::string> additional_params) {
  auto* contextual_session_handle = GetContextualSessionHandle();
  std::vector<const contextual_search::FileInfo*> file_info_list;
  if (contextual_session_handle) {
    // Upload the cached tab context if it exists.
    UploadSnapshotTabContextIfPresent();

    if (input_state_model_) {
      for (auto const& [key, val] :
           // Appends url params for tool and model selection.
           input_state_model_->GetAdditionalQueryParams()) {
        additional_params[key] = val;
      }
    }

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

    file_info_list =
        contextual_session_handle->GetController()->GetFileInfoList();
    // Do not keep tabs in composebox's 'future context to use' since this searchbox handler
    // will be destroyed when this query is submitted and AIM/cobrowsing opens via
    // `CreateSearchUrl`. Tabs are passed to cobrowse composebox through web contents instead.
    contextual_session_handle->CreateSearchUrl(
        std::move(search_url_request_info),
        base::BindOnce(
            [](base::WeakPtr<ContextualSearchboxHandler> self,
               WindowOpenDisposition disposition, GURL url) {
              if (self) {
                self->OpenUrl(url, disposition);
              }
            },
            weak_ptr_factory_.GetWeakPtr(), disposition));
  }

  ClearFiles(/*should_block_auto_suggested_tabs=*/false,
             /*query_submitted=*/true);
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

  // Ensure `input_state_model_` is updated when tab is uploaded.
  if (input_state_model_) {
    input_state_model_->OnContextChanged();
  }
}

void ContextualSearchboxHandler::SnapshotTabContext(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (contextual_session_handle) {
    context_input_data_ =
        contextual_session_handle->GetUploadedContextFileInfos().size() > 0
            ? std::nullopt
            : std::optional(*page_content_data);
  }
  tab_context_snapshot_.emplace(context_token, std::move(page_content_data));

  page_->OnContextualInputStatusChanged(
      context_token, contextual_search::ContextUploadStatus::kProcessing,
      std::nullopt);
}

void ContextualSearchboxHandler::UploadTabContext(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  auto* contextual_session_handle = GetContextualSessionHandle();

  if (contextual_session_handle) {
    context_input_data_ = std::nullopt;
    contextual_session_handle->StartTabContextUploadFlow(
        context_token, std::move(page_content_data),
        CreateImageEncodingOptions());
  }
}

void ContextualSearchboxHandler::OpenUrl(
    GURL url,
    const WindowOpenDisposition disposition) {
  if (!url.is_valid()) {
    return;
  }

  auto* contextual_session_handle = GetContextualSessionHandle();

  auto* contextual_session_service =
      ContextualSearchServiceFactory::GetForProfile(profile_);
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      new_contextual_session_handle = contextual_session_service->GetSession(
          contextual_session_handle->session_id(),
          contextual_session_handle->invocation_source());
  new_contextual_session_handle->set_submitted_context_tokens(
      contextual_session_handle->GetSubmittedContextTokens());

  // TODO(crbug.com/470404040): Determine what to do with the return
  // value of this call, or move this call to a different location.
  new_contextual_session_handle->CheckSearchContentSharingSettings(
      profile_->GetPrefs());

  std::unique_ptr<contextual_search::InputStateModel> new_input_state_model;
  if (input_state_model_) {
    new_input_state_model =
        std::make_unique<contextual_search::InputStateModel>(
            *input_state_model_, *new_contextual_session_handle);
  }

  std::vector<int32_t> selected_tab_ids;
  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    selected_tab_ids = GetSelectedTabIds();
  }

  auto navigation_handle_callback = base::BindOnce(
      [](std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
             handle,
         std::unique_ptr<contextual_search::InputStateModel> input_state_model,
         std::vector<int32_t> selected_tab_ids,
         content::NavigationHandle& navigation_handle) {
        content::WebContents* new_web_contents =
            navigation_handle.GetWebContents();
        ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
            new_web_contents)
            ->SetTaskSession(std::nullopt, std::move(handle),
                             std::move(input_state_model),
                             std::move(selected_tab_ids));
      },
      std::move(new_contextual_session_handle),
      std::move(new_input_state_model), std::move(selected_tab_ids));
  // TODO(crbug.com/469137247): Consider moving this logic to the specific
  // subclasses that have aim navigation.
  bool should_open_url = true;
#if !BUILDFLAG(IS_ANDROID)
  if (OmniboxPopupWebContentsHelper::FromWebContents(web_contents_.get())) {
    should_open_url = false;
    // For the omnibox navigation case, the active tab's web contents differs
    // from the omnibox one. We transfer the session by creating a new handle
    // (copied from the omnibox handle) and assigning it to the active tab.
    auto* browser_window_interface =
        webui::GetBrowserWindowInterface(web_contents_);
    content::OpenURLParams params(url, content::Referrer(), disposition,
                                  ui::PAGE_TRANSITION_LINK, false);
    // If the current tab is part of the context list, navigate in the lens side
    // panel if co-browsing is disabled.
    auto* tab_list = TabListInterface::From(browser_window_interface);
    auto* active_tab = tab_list ? tab_list->GetActiveTab() : nullptr;
    auto* active_web_contents =
        active_tab ? active_tab->GetContents() : nullptr;

    if (ShouldOpenInLensSidePanel(active_web_contents,
                                  contextual_session_handle)) {
      // Open in AIM in lens side panel.
      if (auto* lens_search_controller =
              LensSearchController::FromWebUIWebContents(active_web_contents)) {
        // There technically might not be a match associated with this query
        // since a user can submit a query with just a file.
        std::string query_text;
        net::GetValueForKeyInQuery(url, "q", &query_text);
        lens_search_controller->IssueContextualSearchRequest(
            lens::LensOverlayInvocationSource::kOmniboxContextualQuery, url,
            query_text.empty()
                ? AutocompleteMatchType::Type::SEARCH_SUGGEST
                : AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
            /*is_zero_prefix_suggestion=*/query_text.empty());
        active_web_contents->Focus();
        contextual_session_handle->ClearSubmittedContextTokens();
        return;
      }
    }

    auto* target_web_contents = active_web_contents->OpenURL(
        params, std::move(navigation_handle_callback));

    // Manually set the focus to the newly navigated content. Without this,
    // the focus is re-captured by the Omnibox after query submission (see:
    // http://crbug.com/469458346).
    if (target_web_contents &&
        disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
      target_web_contents->Focus();
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  if (should_open_url) {
    content::OpenURLParams params(url, content::Referrer(), disposition,
                                  ui::PAGE_TRANSITION_LINK, false);
    web_contents_->OpenURL(params, std::move(navigation_handle_callback));
  }
  contextual_session_handle->ClearSubmittedContextTokens();
}

std::optional<base::Uuid> ContextualSearchboxHandler::GetTaskId() {
  if (!web_contents_) {
    return std::nullopt;
  }
  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents_);
  return helper ? helper->task_id() : std::nullopt;
}

contextual_tasks::ActiveTaskContextProvider*
ContextualSearchboxHandler::GetActiveTaskContextProvider() {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  return browser_window_interface
             ? contextual_tasks::ActiveTaskContextProvider::From(
                   browser_window_interface)
             : nullptr;
}
