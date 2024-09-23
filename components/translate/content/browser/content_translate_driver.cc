// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_driver.h"

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
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace translate {

namespace {

// The maximum number of attempts we'll do to see if the page has finshed
// loading before giving up the translation
const int kMaxTranslateLoadCheckAttempts = 20;

// Overrides the hrefTranslate logic to auto-translate when the navigation is
// from any origin rather than only Google origins. Used for manual testing
// where the test page may reside on a test domain.
BASE_FEATURE(kAutoHrefTranslateAllOrigins,
             "AutoHrefTranslateAllOrigins",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

ContentTranslateDriver::ContentTranslateDriver(
    content::WebContents& web_contents,
    language::UrlLanguageHistogram* url_language_histogram)
    : content::WebContentsObserver(&web_contents),
      translate_manager_(nullptr),
      is_otr_context_(web_contents.GetBrowserContext()->IsOffTheRecord()),
      max_reload_check_attempts_(kMaxTranslateLoadCheckAttempts),
      next_page_seq_no_(0),
      language_histogram_(url_language_histogram) {}

ContentTranslateDriver::~ContentTranslateDriver() = default;

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

void ContentTranslateDriver::TranslatePage(int page_seq_no,
                                           const std::string& translate_script,
                                           const std::string& source_lang,
                                           const std::string& target_lang) {
  auto it = translate_agents_.find(page_seq_no);
  if (it == translate_agents_.end())
    return;  // This page has navigated away.

  it->second->TranslateFrame(
      translate_script, source_lang, target_lang,
      base::BindOnce(&ContentTranslateDriver::OnPageTranslated,
                     base::Unretained(this)));
}

void ContentTranslateDriver::RevertTranslation(int page_seq_no) {
  auto it = translate_agents_.find(page_seq_no);
  if (it == translate_agents_.end())
    return;  // This page has navigated away.

  it->second->RevertTranslation();
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
  } else if (navigation_handle->IsInPrimaryMainFrame()) {
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
                                      google_util::ALLOW_NON_STANDARD_PORTS) ||
       IsAutoHrefTranslateAllOriginsEnabled());

  translate_manager_->GetLanguageState()->DidNavigate(
      navigation_handle->IsSameDocument(),
      navigation_handle->IsInPrimaryMainFrame(), reload,
      navigation_handle->GetHrefTranslate(), navigation_from_google);
}

bool ContentTranslateDriver::IsAutoHrefTranslateAllOriginsEnabled() const {
  return base::FeatureList::IsEnabled(kAutoHrefTranslateAllOrigins);
}

void ContentTranslateDriver::OnPageAway(int page_seq_no) {
  translate_agents_.erase(page_seq_no);
}

void ContentTranslateDriver::AddReceiver(
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ContentTranslateDriver::RegisterPage(
    mojo::PendingRemote<translate::mojom::TranslateAgent> translate_agent,
    const translate::LanguageDetectionDetails& details,
    const bool page_level_translation_criteria_met) {
  base::TimeTicks language_determined_time = base::TimeTicks::Now();
  ReportLanguageDeterminedDuration(finish_navigation_time_,
                                   language_determined_time);

  // If we have a language histogram (i.e. we're not in incognito), update it
  // with the detected language of every page visited.
  if (language_histogram_ && details.is_model_reliable)
    language_histogram_->OnPageVisited(details.model_detected_language);

  translate_agents_[++next_page_seq_no_].Bind(std::move(translate_agent));
  translate_agents_[next_page_seq_no_].set_disconnect_handler(
      base::BindOnce(&ContentTranslateDriver::OnPageAway,
                     base::Unretained(this), next_page_seq_no_));
  translate_manager_->set_current_seq_no(next_page_seq_no_);

  translate_manager_->GetLanguageState()->LanguageDetermined(
      details.adopted_language, page_level_translation_criteria_met);

  if (web_contents()) {
    translate_manager_->InitiateTranslation(details.adopted_language);

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
  }

  for (auto& observer : language_detection_observers())
    observer.OnLanguageDetermined(details);

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

}  // namespace translate
