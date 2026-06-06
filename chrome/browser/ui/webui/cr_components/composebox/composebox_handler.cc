// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_url_utils.h"
#include "components/metrics/metrics_provider.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "content/public/browser/page_navigator.h"
#include "net/base/url_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#endif

namespace {

class ComposeboxOmniboxClient final : public ContextualOmniboxClient {
 public:
  ComposeboxOmniboxClient(Profile* profile,
                          content::WebContents* web_contents,
                          ComposeboxHandler* composebox_handler);

  ~ComposeboxOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  std::optional<lens::ContextualInputData> GetContextualInputData()
      const override;

  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;

 private:
  raw_ptr<ComposeboxHandler> composebox_handler_;
};

ComposeboxOmniboxClient::ComposeboxOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents,
    ComposeboxHandler* composebox_handler)
    : ContextualOmniboxClient(profile, web_contents),
      composebox_handler_(composebox_handler) {}

ComposeboxOmniboxClient::~ComposeboxOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
ComposeboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return metrics::OmniboxEventProto::NTP_COMPOSEBOX;
}

std::optional<lens::ContextualInputData>
ComposeboxOmniboxClient::GetContextualInputData() const {
  if (composebox_handler_) {
    return composebox_handler_->context_input_data();
  }
  return std::nullopt;
}

void ComposeboxOmniboxClient::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  const std::map<std::string, std::string>& additional_params =
      lens::GetParametersMapWithoutQuery(destination_url);

  std::string query_text;
  net::GetValueForKeyInQuery(destination_url, "q", &query_text);
  composebox_handler_->SubmitQuery(
      query_text, disposition,
      PageClassificationToAimEntryPoint(
          GetPageClassification(/*is_prefetch=*/false)),
      additional_params);
}

}  // namespace

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    Profile* profile,
    content::WebContents* web_contents,
    GetSessionHandleCallback get_session_callback,
    ClearSessionHandleCallback clear_session_callback)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          std::move(pending_searchbox_page),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<ComposeboxOmniboxClient>(profile,
                                                        web_contents,
                                                        this)),
          std::move(get_session_callback),
          std::move(clear_session_callback)) {}

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<OmniboxController> controller,
    GetSessionHandleCallback get_session_callback,
    ClearSessionHandleCallback clear_session_callback)
    : ContextualSearchboxHandler(std::move(pending_searchbox_handler),
                                 std::move(pending_searchbox_page),
                                 profile,
                                 web_contents,
                                 std::move(controller),
                                 std::move(get_session_callback)),
      web_contents_(web_contents),
      clear_session_callback_(std::move(clear_session_callback)),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {
  // Set the callback for getting suggest inputs from the session.
  // The session is owned by WebUI controller and accessed via callback.
  // It is safe to use Unretained because omnibox client is owned by `this`.
  static_cast<ContextualOmniboxClient*>(omnibox_controller()->client())
      ->SetSuggestInputsCallback(base::BindRepeating(
          &ComposeboxHandler::GetSuggestInputs, base::Unretained(this)));
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

ComposeboxHandler::~ComposeboxHandler() = default;

void ComposeboxHandler::FocusChanged(bool focused) {
  // Unimplemented. Currently the composebox session is tied to when it is
  // connected/disconnected from the DOM, so this is not needed.
}

void ComposeboxHandler::ClearSessionHandle() {
  if (clear_session_callback_) {
    clear_session_callback_.Run();
  }
}

void ComposeboxHandler::HandleLensButtonClick() {
  // Ignore, intentionally unimplemented for NTP.
}

void ComposeboxHandler::HandleFileUpload(bool is_image) {
  // Ignore, intentionally unimplemented for NTP.
}

void ComposeboxHandler::StartPlatformVoiceRecognition() {
  // Ignore, intentionally unimplemented for NTP.
}

void ComposeboxHandler::OnContextMenuOpened() {
  if (contextual_tasks::GetIsContextualTasksLazyFetchClusterInfoEnabled()) {
    auto* session_handle = GetContextualSessionHandle();
    if (session_handle && session_handle->GetController()) {
      session_handle->GetController()->TriggerFetchClusterInfo();
    }
  }
}

void ComposeboxHandler::NotifyComposeboxQuerySubmittedWithContext() {
#if !BUILDFLAG(IS_ANDROID)
  if (!web_contents_) {
    return;
  }
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!browser_window_interface) {
    return;
  }
  auto* user_education_interface =
      BrowserUserEducationInterface::From(browser_window_interface);
  if (!user_education_interface) {
    return;
  }
  user_education_interface->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHDesktopRealboxContextualSearchFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ComposeboxHandler::NavigateUrl(const GURL& url) {
  if (!url.is_valid()) {
    return;
  }
  content::WebContents* current_web_contents = web_contents_.get();
  if (!current_web_contents) {
    return;
  }
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(current_web_contents);
  if (!browser_window_interface) {
    return;
  }
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  browser_window_interface->OpenURL(std::move(params),
                                    /*navigation_handle_callback=*/{});
}

void ComposeboxHandler::CloseLensOverlayFromWebUI(
    composebox::mojom::LensOverlayDismissalSource dismissal_source) {
  // Ignore, intentionally unimplemented for NTP.
}

// Required by composebox::mojom::PageHandler. Delegates to the base class
// ContextualSearchboxHandler which does not implement that interface.
void ComposeboxHandler::SetSmartTabSharingActive(bool active) {
  ContextualSearchboxHandler::SetSmartTabSharingActive(active);
}

// Required by composebox::mojom::PageHandler. Delegates to the base class
// ContextualSearchboxHandler which does not implement that interface.
void ComposeboxHandler::GetSmartTabSharingActive(
    GetSmartTabSharingActiveCallback callback) {
  ContextualSearchboxHandler::GetSmartTabSharingActive(std::move(callback));
}

void ComposeboxHandler::ExecuteAction(uint8_t line,
                                      uint8_t action_index,
                                      const GURL& url,
                                      base::TimeTicks match_selection_timestamp,
                                      uint8_t mouse_button,
                                      bool alt_key,
                                      bool ctrl_key,
                                      bool meta_key,
                                      bool shift_key) {
  mojo::ReportBadMessage("Composebox does not have actions");
}

void ComposeboxHandler::OnThumbnailRemoved() {
  mojo::ReportBadMessage("No thumbnails in composebox input");
}

void ComposeboxHandler::ClearFiles(bool should_block_auto_suggested_tabs) {
  ContextualSearchboxHandler::ClearFiles(should_block_auto_suggested_tabs);
  // Reset the AIM tool mode to not include file upload if it currently does.
  if (GetInputState().active_tool ==
      omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD) {
    input_state_model_->setActiveTool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  }
}

void ComposeboxHandler::SubmitQuery(const std::string& query_text,
                                    uint8_t mouse_button,
                                    bool alt_key,
                                    bool ctrl_key,
                                    bool meta_key,
                                    bool shift_key) {
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  omnibox::ChromeAimEntryPoint aim_entry_point =
      PageClassificationToAimEntryPoint(
          omnibox_controller()->client()->GetPageClassification(
              /*is_prefetch=*/false));

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    int file_count = 0;
    if (auto* session_handle = GetContextualSessionHandle()) {
      file_count = session_handle->GetUploadedContextFileInfos().size();
    }
    metrics_recorder->RecordNoAcMatchSubmitQuery(query_text.size(), file_count,
                                                 /*is_ac_match=*/false);
  }

  SubmitQuery(query_text, disposition, aim_entry_point,
              /*additional_params=*/{});
}

void ComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    omnibox::ChromeAimEntryPoint aim_entrypoint,
    std::map<std::string, std::string> additional_params) {
  if (auto* metrics_recorder = GetMetricsRecorder()) {
    // Record AIM tool and model mode on query submission.
    const auto& input_state = GetInputState();
    std::vector<omnibox::InputType> active_input_types =
        contextual_search::InputStateModel::GetCurrentInputTypes(
            GetContextualSessionHandle());
    metrics_recorder->RecordModesOnSubmission(
        input_state.active_tool, input_state.active_model, active_input_types);
  }

  ContextualizeQueryAndOpenUrl(query_text, disposition, aim_entrypoint,
                               std::move(additional_params));
}

void ComposeboxHandler::OpenUrl(GURL url,
                                const WindowOpenDisposition disposition) {
  ContextualSearchboxHandler::OpenUrl(url, disposition);
  // To keep the current composebox in a valid state after passing along its
  // session handle and input state model, clear both of these values. This
  // way the state will reset on the next use of the composebox. Clear the
  // session handle before initializing the input state model, so the model
  // gets a fresh handle.
  ResetInputStateModel();
  ClearSessionHandle();
  InitializeInputStateModel();
  // This is technically wrong, it'll start a new session for the composebox
  // even before its been opened. This is needed to re-request the cluster info
  // for the composebox.
  // TODO(crbug.com/491871526): Re-request cluster info when needed, not on
  // navigation.
  auto* contextual_session_handle = GetContextualSessionHandle();
  if (contextual_session_handle) {
    contextual_session_handle->NotifySessionStarted();
  }
}
