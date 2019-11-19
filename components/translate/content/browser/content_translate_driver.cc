// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_driver.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/translate/content/browser/content_record_page_language.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/ukm/content/source_url_recorder.h"
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
#include "content/public/common/web_preferences.h"
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
const base::Feature kAutoHrefTranslateAllOrigins{
    "AutoHrefTranslateAllOrigins", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

ContentTranslateDriver::ContentTranslateDriver(
    content::NavigationController* nav_controller,
    language::UrlLanguageHistogram* url_language_histogram)
    : content::WebContentsObserver(nav_controller->GetWebContents()),
      navigation_controller_(nav_controller),
      translate_manager_(nullptr),
      max_reload_check_attempts_(kMaxTranslateLoadCheckAttempts),
      next_page_seq_no_(0),
      language_histogram_(url_language_histogram) {
  DCHECK(navigation_controller_);
}

ContentTranslateDriver::~ContentTranslateDriver() {}

void ContentTranslateDriver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ContentTranslateDriver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ContentTranslateDriver::InitiateTranslation(const std::string& page_lang,
                                                 int attempt) {
  if (translate_manager_->GetLanguageState().translation_pending())
    return;

  // During a reload we need web content to be available before the
  // translate script is executed. Otherwise we will run the translate script on
  // an empty DOM which will fail. Therefore we wait a bit to see if the page
  // has finished.
  if (web_contents()->IsLoading() && attempt < max_reload_check_attempts_) {
    int backoff = attempt * kMaxTranslateLoadCheckAttempts;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContentTranslateDriver::InitiateTranslation,
                       weak_pointer_factory_.GetWeakPtr(), page_lang,
                       attempt + 1),
        base::TimeDelta::FromMilliseconds(backoff));
    return;
  }

  translate_manager_->InitiateTranslation(
      translate::TranslateDownloadManager::GetLanguageCode(page_lang));
}

// TranslateDriver methods

bool ContentTranslateDriver::IsLinkNavigation() {
  return navigation_controller_ &&
         navigation_controller_->GetLastCommittedEntry() &&
         ui::PageTransitionCoreTypeIs(
             navigation_controller_->GetLastCommittedEntry()
                 ->GetTransitionType(),
             ui::PAGE_TRANSITION_LINK);
}

void ContentTranslateDriver::OnTranslateEnabledChanged() {
  content::WebContents* web_contents = navigation_controller_->GetWebContents();
  for (auto& observer : observer_list_)
    observer.OnTranslateEnabledChanged(web_contents);
}

void ContentTranslateDriver::OnIsPageTranslatedChanged() {
  content::WebContents* web_contents = navigation_controller_->GetWebContents();
  for (auto& observer : observer_list_)
    observer.OnIsPageTranslatedChanged(web_contents);
}

void ContentTranslateDriver::TranslatePage(int page_seq_no,
                                           const std::string& translate_script,
                                           const std::string& source_lang,
                                           const std::string& target_lang) {
  auto it = pages_.find(page_seq_no);
  if (it == pages_.end())
    return;  // This page has navigated away.

  it->second->Translate(
      translate_script, source_lang, target_lang,
      base::BindOnce(&ContentTranslateDriver::OnPageTranslated,
                     base::Unretained(this)));
}

void ContentTranslateDriver::RevertTranslation(int page_seq_no) {
  auto it = pages_.find(page_seq_no);
  if (it == pages_.end())
    return;  // This page has navigated away.

  it->second->RevertTranslation();
}

bool ContentTranslateDriver::IsIncognito() {
  return navigation_controller_->GetBrowserContext()->IsOffTheRecord();
}

const std::string& ContentTranslateDriver::GetContentsMimeType() {
  return navigation_controller_->GetWebContents()->GetContentsMimeType();
}

const GURL& ContentTranslateDriver::GetLastCommittedURL() {
  return navigation_controller_->GetWebContents()->GetLastCommittedURL();
}

const GURL& ContentTranslateDriver::GetVisibleURL() {
  return navigation_controller_->GetWebContents()->GetVisibleURL();
}

ukm::SourceId ContentTranslateDriver::GetUkmSourceId() {
  return ukm::GetSourceIdForWebContentsDocument(
      navigation_controller_->GetWebContents());
}

bool ContentTranslateDriver::HasCurrentPage() {
  return (navigation_controller_->GetLastCommittedEntry() != nullptr);
}

void ContentTranslateDriver::OpenUrlInNewTab(const GURL& url) {
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  navigation_controller_->GetWebContents()->OpenURL(params);
}

// content::WebContentsObserver methods

void ContentTranslateDriver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Check whether this is a reload: When doing a page reload, the
  // TranslateLanguageDetermined IPC is not sent so the translation needs to be
  // explicitly initiated.

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  // If the navigation happened while offline don't show the translate
  // bar since there will be nothing to translate.
  if (load_details.http_status_code == 0 ||
      load_details.http_status_code == net::HTTP_INTERNAL_SERVER_ERROR) {
    return;
  }

  if (!load_details.is_main_frame &&
      translate_manager_->GetLanguageState().translation_declined()) {
    // Some sites (such as Google map) may trigger sub-frame navigations
    // when the user interacts with the page.  We don't want to show a new
    // infobar if the user already dismissed one in that case.
    return;
  }

  // If not a reload, return.
  if (!ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                    ui::PAGE_TRANSITION_RELOAD) &&
      load_details.type != content::NAVIGATION_TYPE_SAME_PAGE) {
    return;
  }

  if (entry->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK) {
    // Workaround for http://crbug.com/653051: back navigation sometimes have
    // the reload core type. Once http://crbug.com/669008 got resolved, we
    // could revisit here for a thorough solution.
    //
    // This means that the new translation won't be started when the page
    // is restored from back-forward cache, which is the right thing to do.
    // TODO(crbug.com/1001087): Ensure that it stays disabled for
    // back-forward navigations even when bug above is fixed.
    return;
  }

  if (!translate_manager_->GetLanguageState().page_needs_translation())
    return;

  // Note that we delay it as the ordering of the processing of this callback
  // by WebContentsObservers is undefined and might result in the current
  // infobars being removed. Since the translation initiation process might add
  // an infobar, it must be done after that.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContentTranslateDriver::InitiateTranslation,
                     weak_pointer_factory_.GetWeakPtr(),
                     translate_manager_->GetLanguageState().original_language(),
                     0));
}

void ContentTranslateDriver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  // Let the LanguageState clear its state.
  const bool reload =
      navigation_handle->GetReloadType() != content::ReloadType::NONE ||
      navigation_handle->IsSameDocument();

  const base::Optional<url::Origin>& initiator_origin =
      navigation_handle->GetInitiatorOrigin();

  bool navigation_from_google =
      initiator_origin.has_value() &&
      (google_util::IsGoogleDomainUrl(initiator_origin->GetURL(),
                                      google_util::DISALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS) ||
       base::FeatureList::IsEnabled(kAutoHrefTranslateAllOrigins));

  translate_manager_->GetLanguageState().DidNavigate(
      navigation_handle->IsSameDocument(), navigation_handle->IsInMainFrame(),
      reload, navigation_handle->GetHrefTranslate(), navigation_from_google);
}

void ContentTranslateDriver::OnPageAway(int page_seq_no) {
  pages_.erase(page_seq_no);
}

void ContentTranslateDriver::AddReceiver(
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ContentTranslateDriver::RegisterPage(
    mojo::PendingRemote<translate::mojom::Page> page,
    const translate::LanguageDetectionDetails& details,
    const bool page_needs_translation) {
  // If we have a language histogram (i.e. we're not in incognito), update it
  // with the detected language of every page visited.
  if (language_histogram_ && details.is_cld_reliable)
    language_histogram_->OnPageVisited(details.cld_language);

  pages_[++next_page_seq_no_].Bind(std::move(page));
  pages_[next_page_seq_no_].set_disconnect_handler(
      base::BindOnce(&ContentTranslateDriver::OnPageAway,
                     base::Unretained(this), next_page_seq_no_));
  translate_manager_->set_current_seq_no(next_page_seq_no_);

  translate_manager_->GetLanguageState().LanguageDetermined(
      details.adopted_language, page_needs_translation);

  if (web_contents()) {
    translate_manager_->InitiateTranslation(details.adopted_language);

    // Save the page language on the navigation entry so it can be synced.
    auto* const entry = web_contents()->GetController().GetLastCommittedEntry();
    if (entry != nullptr)
      SetPageLanguageInNavigation(details.adopted_language, entry);
  }

  for (auto& observer : observer_list_)
    observer.OnLanguageDetermined(details);
}

void ContentTranslateDriver::OnPageTranslated(
    bool cancelled,
    const std::string& original_lang,
    const std::string& translated_lang,
    TranslateErrors::Type error_type) {
  if (cancelled)
    return;

  translate_manager_->PageTranslated(
      original_lang, translated_lang, error_type);
  for (auto& observer : observer_list_)
    observer.OnPageTranslated(original_lang, translated_lang, error_type);
}

}  // namespace translate
