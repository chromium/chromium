// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_driver.h"

#include "pdf/buildflags.h"
#if BUILDFLAG(ENABLE_PDF)
#include "components/translate/content/browser/pdf_translation_coordinator.h"
#endif

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/translate/content/browser/content_record_page_language.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_features.h"
#include "components/translate/core/common/translate_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace translate {

namespace {

// The maximum number of attempts we'll do to see if the page has finished
// loading before giving up the translation
const int kMaxTranslateLoadCheckAttempts = 20;

bool IsReadingModeSidePanel(const GURL& url) {
  return url.SchemeIs(content::kChromeUIUntrustedScheme) &&
         url.host() == kReadingModeSidePanelHost;
}

class ContentTranslateDriverUserData : public base::SupportsUserData::Data {
 public:
  explicit ContentTranslateDriverUserData(base::WeakPtr<ContentTranslateDriver> driver) : driver_(driver) {}
  ContentTranslateDriver* driver() const { return driver_.get(); }
 private:
  const base::WeakPtr<ContentTranslateDriver> driver_;
};

const void* const kContentTranslateDriverUserDataKey = &kContentTranslateDriverUserDataKey;

}  // namespace

// static
ContentTranslateDriver* ContentTranslateDriver::FromWebContents(content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  auto* data = static_cast<ContentTranslateDriverUserData*>(
      web_contents->GetUserData(kContentTranslateDriverUserDataKey));
  return data ? data->driver() : nullptr;
}

ContentTranslateDriver::ContentTranslateDriver(
    content::WebContents& web_contents,
    language::UrlLanguageHistogram* url_language_histogram)
    : content::WebContentsObserver(&web_contents),
      translate_manager_(nullptr),
      is_otr_context_(web_contents.GetBrowserContext()->IsOffTheRecord()),
      max_reload_check_attempts_(kMaxTranslateLoadCheckAttempts),
      last_registered_page_id_(ukm::kInvalidSourceId),
      active_page_seq_no_(0),
      next_page_seq_no_(0),
      language_histogram_(url_language_histogram) {
  web_contents.SetUserData(
      kContentTranslateDriverUserDataKey,
      std::make_unique<ContentTranslateDriverUserData>(
          weak_pointer_factory_.GetWeakPtr()));
}

ContentTranslateDriver::~ContentTranslateDriver() {
  // Reset `translate_manager_` first as a safeguard to ensure the raw pointer
  // is cleared before dismantling user data and observer state during
  // destruction.
  translate_manager_ = nullptr;

  if (web_contents()) {
    web_contents()->RemoveUserData(kContentTranslateDriverUserDataKey);
  }
  // Clear any remaining observers to avoid the CHECK in ObserverList's
  // destructor. This can happen if the WebContents is destroyed before
  // observers have a chance to unregister (e.g., Java-side cleanup callbacks
  // may not fire in time during WebContents destruction).
  // See https://crbug.com/474819145.
  translation_observers_.Clear();
}

void ContentTranslateDriver::AddTranslationObserver(
    TranslationObserver* observer) {
  translation_observers_.AddObserver(observer);
}

void ContentTranslateDriver::RemoveTranslationObserver(
    TranslationObserver* observer) {
  translation_observers_.RemoveObserver(observer);
}

void ContentTranslateDriver::InitiateTranslation(const std::string& page_lang,
                                                 int attempt) {
  if (translate_manager_->GetLanguageState()->translation_pending())
    return;

  // During a reload we need web content to be available before the
  // translate script is executed. Otherwise we will run the translate script on
  // an empty DOM which will fail. Therefore we wait a bit to see if the page
  // has finished.
  if (web_contents()->IsLoading() && attempt < max_reload_check_attempts_) {
    int backoff = attempt * kMaxTranslateLoadCheckAttempts;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContentTranslateDriver::InitiateTranslation,
                       weak_pointer_factory_.GetWeakPtr(), page_lang,
                       attempt + 1),
        base::Milliseconds(backoff));
    return;
  }

  translate_manager_->InitiateTranslation(
      translate::TranslateDownloadManager::GetLanguageCode(page_lang));
}

// TranslateDriver methods

bool ContentTranslateDriver::IsLinkNavigation() {
  return ui::PageTransitionCoreTypeIs(web_contents()
                                          ->GetController()
                                          .GetLastCommittedEntry()
                                          ->GetTransitionType(),
                                      ui::PAGE_TRANSITION_LINK);
}

void ContentTranslateDriver::OnTranslateEnabledChanged() {
  for (auto& observer : translation_observers_)
    observer.OnTranslateEnabledChanged(web_contents());
}

void ContentTranslateDriver::OnIsPageTranslatedChanged() {
  for (auto& observer : translation_observers_)
    observer.OnIsPageTranslatedChanged(web_contents());
}

mojom::TranslateAgent* ContentTranslateDriver::GetTranslateAgent(
    int page_seq_no) {
  std::map<int, PageAgents>::iterator it = translate_agents_.find(page_seq_no);
  if (it == translate_agents_.end()) {
    return nullptr;  // This page has navigated away.
  }

  return it->second.side_panel_agent.is_bound() &&
                 GetContentsMimeType() == "application/pdf"
             ? it->second.side_panel_agent.get()
             : it->second.main_agent.get();
}

void ContentTranslateDriver::TranslatePage(int page_seq_no,
                                           std::string_view translate_script,
                                           std::string_view source_lang,
                                           std::string_view target_lang) {
  mojom::TranslateAgent* agent = GetTranslateAgent(page_seq_no);
  if (!agent) {
    return;
  }

  agent->TranslateFrame(
      std::string(translate_script), std::string(source_lang),
      std::string(target_lang),
      base::BindOnce(&ContentTranslateDriver::OnPageTranslated,
                     base::Unretained(this)));
}

void ContentTranslateDriver::RevertTranslation(int page_seq_no) {
  std::map<int, PageAgents>::iterator it = translate_agents_.find(page_seq_no);
  if (it == translate_agents_.end())
    return;  // This page has navigated away.

  if (it->second.main_agent.is_bound()) {
    it->second.main_agent->RevertTranslation();
  }

  if (it->second.side_panel_agent.is_bound()) {
    it->second.side_panel_agent->RevertTranslation();
  }
}

bool ContentTranslateDriver::IsIncognito() const {
  return is_otr_context_;
}

const std::string& ContentTranslateDriver::GetContentsMimeType() {
  return web_contents()->GetContentsMimeType();
}

const GURL& ContentTranslateDriver::GetLastCommittedURL() const {
  return last_committed_url_;
}

const GURL& ContentTranslateDriver::GetVisibleURL() {
  return web_contents()->GetVisibleURL();
}

ukm::SourceId ContentTranslateDriver::GetUkmSourceId() {
  return web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

bool ContentTranslateDriver::HasCurrentPage() const {
  // TODO(crbug.com/40432764): This method previously checked for the existence
  // of GetLastCommittedEntry(), which always exists now. Check if this is true
  // for other implementations and consider removing this method.
  return true;
}

void ContentTranslateDriver::InitiateTranslationIfReload(
    content::NavigationHandle* navigation_handle) {
  // Check whether this is a reload: When doing a page reload, the
  // TranslateLanguageDetermined IPC is not sent so the translation needs to be
  // explicitly initiated.

  // If the navigation happened while offline don't show the translate
  // bar since there will be nothing to translate.
  int response_code =
      navigation_handle->GetResponseHeaders()
          ? navigation_handle->GetResponseHeaders()->response_code()
          : 0;
  if (response_code == 0 || response_code == net::HTTP_INTERNAL_SERVER_ERROR)
    return;

  if (!navigation_handle->IsInMainFrame() &&
      translate_manager_->GetLanguageState()->translation_declined()) {
    // Some sites (such as Google map) may trigger sub-frame navigations
    // when the user interacts with the page.  We don't want to show a new
    // infobar if the user already dismissed one in that case.
    return;
  }

  // If not a reload, return.
  if (navigation_handle->GetReloadType() == content::ReloadType::NONE)
    return;

  if (navigation_handle->GetPageTransition() &
      ui::PAGE_TRANSITION_FORWARD_BACK) {
    // Workaround for http://crbug.com/653051: back navigation sometimes have
    // the reload core type. Once http://crbug.com/669008 got resolved, we
    // could revisit here for a thorough solution.
    //
    // This means that the new translation won't be started when the page
    // is restored from back-forward cache, which is the right thing to do.
    // TODO(crbug.com/40097545): Ensure that it stays disabled for
    // back-forward navigations even when bug above is fixed.
    return;
  }

  if (!translate_manager_->GetLanguageState()
           ->page_level_translation_criteria_met()) {
    return;
  }

  // Note that we delay it as the ordering of the processing of this callback
  // by WebContentsObservers is undefined and might result in the current
  // infobars being removed. Since the translation initiation process might add
  // an infobar, it must be done after that.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContentTranslateDriver::InitiateTranslation,
                     weak_pointer_factory_.GetWeakPtr(),
                     translate_manager_->GetLanguageState()->source_language(),
                     0));
}

bool ContentTranslateDriver::IsPdfTranslation() {
  return GetContentsMimeType() == kPdfMimeType &&
         base::FeatureList::IsEnabled(translate::kEnableTranslatePdf);
}

// content::WebContentsObserver methods
void ContentTranslateDriver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  // Continue to process the navigation only if it is for the primary main
  // frame. It is safe to do so because:
  // - A non-primary page should not reset `this`'s language state since the
  // state is set for the primary page. It will be allowed to update the state
  // after it becomes the primary page (at that time, this function will be
  // invoked again, and the page will update the state).
  // - This class does not need to handle subframe navigations. Employing this
  // class means the flag of kTranslateSubFrames is disabled, i.e., subframe
  // translation is not supported. Besides it, subframes cannot change language
  // state.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Store the main frame committed URL.
  last_committed_url_ = web_contents()->GetLastCommittedURL();

  InitiateTranslationIfReload(navigation_handle);

  if (navigation_handle->IsPrerenderedPageActivation()) {
    // Set it to NULL time, and do not report the LanguageDeterminedDuration
    // metric in this case.
    // The browser defers the RegisterPage() message on a prerendering page, so
    // this kind of data is noisy and should be filtered out.
    finish_navigation_time_ = base::TimeTicks();
  } else {
    finish_navigation_time_ = base::TimeTicks::Now();
  }

  // Let the LanguageState clear its state.
  const bool reload =
      navigation_handle->GetReloadType() != content::ReloadType::NONE ||
      navigation_handle->IsSameDocument();

  const std::optional<url::Origin>& initiator_origin =
      navigation_handle->GetInitiatorOrigin();

  bool navigation_from_google =
      initiator_origin.has_value() &&
      (google_util::IsGoogleDomainUrl(initiator_origin->GetURL(),
                                      google_util::DISALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS));

  translate_manager_->GetLanguageState()->DidNavigate(
      navigation_handle->IsSameDocument(),
      navigation_handle->IsInPrimaryMainFrame(), reload,
      navigation_handle->GetHrefTranslate(), navigation_from_google);
}

void ContentTranslateDriver::OnPageAway(int page_seq_no) {
  std::map<int, PageAgents>::iterator it = translate_agents_.find(page_seq_no);
  if (it == translate_agents_.end()) {
    return;
  }
  it->second.main_agent.reset();
  if (!it->second.side_panel_agent.is_bound()) {
    translate_agents_.erase(it);
  }
}

void ContentTranslateDriver::OnSidePanelAway(int page_seq_no) {
  std::map<int, PageAgents>::iterator it = translate_agents_.find(page_seq_no);
  if (it == translate_agents_.end()) {
    return;
  }
  it->second.side_panel_agent.reset();
  if (!it->second.main_agent.is_bound()) {
    translate_agents_.erase(it);
  }
}

void ContentTranslateDriver::AddReceiver(
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ContentTranslateDriver::RegisterPage(
    mojo::PendingRemote<translate::mojom::TranslateAgent> translate_agent,
    const translate::LanguageDetectionDetails& details,
    const bool page_level_translation_criteria_met) {
  if (!web_contents()) {
    return;
  }
  base::TimeTicks language_determined_time = base::TimeTicks::Now();

  int page_seq_no = UpdatePageSequenceNumber();
  if (IsReadingModeSidePanel(details.url)) {
    BindSidePanelTranslateAgent(page_seq_no, std::move(translate_agent));
    return;
  }
  ReportLanguageDeterminedDuration(finish_navigation_time_,
                                   language_determined_time);

  // If we have a language histogram (i.e. we're not in incognito), update it
  // with the detected language of every page visited.
  if (language_histogram_ && details.is_model_reliable) {
    language_histogram_->OnPageVisited(details.model_detected_language);
  }

  BindMainTranslateAgent(page_seq_no, std::move(translate_agent));

  translate_manager_->GetLanguageState()->LanguageDetermined(
      details.adopted_language, page_level_translation_criteria_met);

  if (IsPdfTranslation()) {
#if BUILDFLAG(ENABLE_PDF)
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    auto* coordinator =
        PDFTranslationCoordinator::GetOrCreateForCurrentDocument(rfh);
    coordinator->RunIfPdfIsTranslatable(base::BindOnce(
        &TranslateManager::InitiateTranslation,
        translate_manager_->GetWeakPtr(), details.adopted_language));
#endif
  } else {
    translate_manager_->InitiateTranslation(details.adopted_language);
  }

    // Save the page language on the navigation entry so it can be synced.
    // TODO(crbug.com/40779913): The mojo IPC coming from the renderer might
    // race with a navigation, so the page that sent this message might already
    // be in the pending delete state after being navigated away from.
    // Rearchitect the renderer-browser Mojo connection to be able to explicitly
    // determine the document/content::Page with which this language
    // determination event is associated, thus avoiding the potential for corner
    // cases where the detected language is attributed to the wrong page.
    auto* const entry = web_contents()->GetController().GetLastCommittedEntry();
    SetPageLanguageInNavigation(details.adopted_language, entry);

  for (LanguageDetectionObserver& observer : language_detection_observers()) {
    observer.OnLanguageDetermined(details);
  }

  translate_manager_->GetActiveTranslateMetricsLogger()
      ->LogHTMLDocumentLanguage(details.html_root_language);
  translate_manager_->GetActiveTranslateMetricsLogger()->LogHTMLContentLanguage(
      details.content_language);
  translate_manager_->GetActiveTranslateMetricsLogger()->LogDetectedLanguage(
      details.model_detected_language);
  translate_manager_->GetActiveTranslateMetricsLogger()
      ->LogDetectionReliabilityScore(details.model_reliability_score);
  translate_manager_->GetActiveTranslateMetricsLogger()->LogWasContentEmpty(
      details.contents.length() > 0);
}
void ContentTranslateDriver::OnPageTranslated(
    bool cancelled,
    const std::string& source_lang,
    const std::string& translated_lang,
    TranslateErrors error_type) {
  if (cancelled) {
    // Informs the |TranslateMetricsLogger| that the translation was cancelled.
    translate_manager_->GetActiveTranslateMetricsLogger()
        ->LogTranslationFinished(false, error_type);
    return;
  }

  translate_manager_->PageTranslated(source_lang, translated_lang, error_type);
  for (auto& observer : translation_observers_)
    observer.OnPageTranslated(source_lang, translated_lang, error_type);
}

int ContentTranslateDriver::UpdatePageSequenceNumber() {
  ukm::SourceId page_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  if (active_page_seq_no_ == 0 || page_id != last_registered_page_id_) {
    last_registered_page_id_ = page_id;
    int old_page_seq_no = active_page_seq_no_;
    active_page_seq_no_ = ++next_page_seq_no_;
    translate_manager_->set_current_seq_no(active_page_seq_no_);

    if (old_page_seq_no != 0) {
      std::map<int, PageAgents>::iterator it =
          translate_agents_.find(old_page_seq_no);
      if (it != translate_agents_.end() &&
          it->second.side_panel_agent.is_bound()) {
        PageAgents& new_agents = translate_agents_[active_page_seq_no_];
        new_agents.side_panel_agent = std::move(it->second.side_panel_agent);
        new_agents.side_panel_agent.set_disconnect_handler(
            base::BindOnce(&ContentTranslateDriver::OnSidePanelAway,
                           base::Unretained(this), active_page_seq_no_));
        if (!it->second.main_agent.is_bound()) {
          translate_agents_.erase(it);
        }
      }
    }
  }
  return active_page_seq_no_;
}

void ContentTranslateDriver::BindSidePanelTranslateAgent(
    int page_seq_no,
    mojo::PendingRemote<mojom::TranslateAgent> translate_agent) {
  auto& agents = translate_agents_[page_seq_no];
  agents.side_panel_agent.reset();
  agents.side_panel_agent.Bind(std::move(translate_agent));
  // Use a specific disconnect handler that doesn't erase the whole entry.
  // base::Unretained(this) is safe here because `side_panel_agent` is owned
  // by `translate_agents_`, which is owned by `this`. The callback is
  // destroyed when the remote is destroyed, guaranteeing `this` outlives it.
  agents.side_panel_agent.set_disconnect_handler(
      base::BindOnce(&ContentTranslateDriver::OnSidePanelAway,
                     base::Unretained(this), page_seq_no));
}

void ContentTranslateDriver::BindMainTranslateAgent(
    int page_seq_no,
    mojo::PendingRemote<mojom::TranslateAgent> translate_agent) {
  auto& agents = translate_agents_[page_seq_no];
  agents.main_agent.reset();
  agents.main_agent.Bind(std::move(translate_agent));
  // base::Unretained(this) is safe here because `main_agent` is owned by
  // `translate_agents_`, which is owned by `this`. The callback is destroyed
  // when the remote is destroyed, guaranteeing `this` outlives it.
  agents.main_agent.set_disconnect_handler(
      base::BindOnce(&ContentTranslateDriver::OnPageAway,
                     base::Unretained(this), page_seq_no));
}
}  // namespace translate
