// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/error_page/common/localized_error.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace {

// Returns whether |error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it).
bool IsNetDnsError(const error_page::Error& error) {
  return error.domain() == error_page::Error::kNetErrorDomain &&
         net::IsHostnameResolutionError(error.reason());
}

}  // namespace

struct NetErrorHelperCore::ErrorPageInfo {
  ErrorPageInfo(error_page::Error error, bool was_failed_post)
      : error(error), was_failed_post(was_failed_post) {}

  // Information about the failed page load.
  error_page::Error error;
  bool was_failed_post;

  // Information about the status of the error page.

  // True if a page is a DNS error page and has not yet received a final DNS
  // probe status.
  bool needs_dns_updates = false;
  bool dns_probe_complete = false;

  // True if a page has completed loading, at which point it can receive
  // updates.
  bool is_finished_loading = false;

  error_page::LocalizedError::PageState page_state;
};

NetErrorHelperCore::NetErrorHelperCore(Delegate* delegate)
    : delegate_(delegate),
      last_probe_status_(error_page::DNS_PROBE_POSSIBLE),
      can_show_network_diagnostics_dialog_(false),
      navigation_from_button_(NO_BUTTON)
#if BUILDFLAG(IS_ANDROID)
      ,
      page_auto_fetcher_helper_(
          std::make_unique<PageAutoFetcherHelper>(delegate->GetRenderFrame()))
#endif
{
}

NetErrorHelperCore::~NetErrorHelperCore() = default;

void NetErrorHelperCore::OnCommitLoad(FrameType frame_type, const GURL& url) {
  if (frame_type != MAIN_FRAME)
    return;

#if BUILDFLAG(IS_ANDROID)
  // Don't need this state. It will be refreshed if another error page is
  // loaded.
  available_content_helper_.Reset();
  page_auto_fetcher_helper_->OnCommitLoad();
#endif

  // Track if an error occurred due to a page button press.
  // This isn't perfect; if (for instance), the server is slow responding
  // to a request generated from the page reload button, and the user hits
  // the browser reload button, this code will still believe the
  // result is from the page reload button.
  if (committed_error_page_info_ && pending_error_page_info_ &&
      navigation_from_button_ != NO_BUTTON &&
      committed_error_page_info_->error.url() ==
          pending_error_page_info_->error.url()) {
    DCHECK(navigation_from_button_ == RELOAD_BUTTON);
    RecordEvent(error_page::NETWORK_ERROR_PAGE_RELOAD_BUTTON_ERROR);
  }
  navigation_from_button_ = NO_BUTTON;

  committed_error_page_info_ = std::move(pending_error_page_info_);
}

void NetErrorHelperCore::ErrorPageLoadedWithFinalErrorCode() {
  ErrorPageInfo* page_info = committed_error_page_info_.get();
  DCHECK(page_info);
  error_page::Error updated_error = GetUpdatedError(*page_info);

  if (page_info->page_state.is_offline_error)
    RecordEvent(error_page::NETWORK_ERROR_PAGE_OFFLINE_ERROR_SHOWN);

#if BUILDFLAG(IS_ANDROID)
  // The fetch functions shouldn't be triggered multiple times per page load.
  if (page_info->page_state.offline_content_feature_enabled) {
    available_content_helper_.FetchAvailableContent(base::BindOnce(
        &Delegate::OfflineContentAvailable, base::Unretained(delegate_)));
  }

  // |TrySchedule()| shouldn't be called more than once per page.
  if (page_info->page_state.auto_fetch_allowed) {
    page_auto_fetcher_helper_->TrySchedule(
        false, base::BindOnce(&Delegate::SetAutoFetchState,
                              base::Unretained(delegate_)));
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (page_info->page_state.download_button_shown)
    RecordEvent(error_page::NETWORK_ERROR_PAGE_DOWNLOAD_BUTTON_SHOWN);

  if (page_info->page_state.reload_button_shown)
    RecordEvent(error_page::NETWORK_ERROR_PAGE_RELOAD_BUTTON_SHOWN);

  delegate_->SetIsShowingDownloadButton(
      page_info->page_state.download_button_shown);
}

void NetErrorHelperCore::OnFinishLoad(FrameType frame_type) {
  if (frame_type != MAIN_FRAME)
    return;

  if (!committed_error_page_info_)
    return;

  committed_error_page_info_->is_finished_loading = true;

  RecordEvent(error_page::NETWORK_ERROR_PAGE_SHOWN);

  delegate_->SetIsShowingDownloadButton(
      committed_error_page_info_->page_state.download_button_shown);
  delegate_->EnablePageHelperFunctions();

  DVLOG(1) << "Error page finished loading; sending saved status.";
  if (committed_error_page_info_->needs_dns_updates) {
    if (last_probe_status_ != error_page::DNS_PROBE_POSSIBLE)
      UpdateErrorPage();
  } else {
    ErrorPageLoadedWithFinalErrorCode();
  }
}

void NetErrorHelperCore::PrepareErrorPage(
    FrameType frame_type,
    const error_page::Error& error,
    bool is_failed_post,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (frame_type == MAIN_FRAME) {
    pending_error_page_info_ =
        std::make_unique<ErrorPageInfo>(error, is_failed_post);
    PrepareErrorPageForMainFrame(pending_error_page_info_.get(),
                                 std::move(alternative_error_page_info),
                                 error_html);
  } else if (error_html) {
    delegate_->GenerateLocalizedErrorPage(
        error, is_failed_post,
        false /* No diagnostics dialogs allowed for subframes. */,
        std::move(alternative_error_page_info), error_html);
  }
}

void NetErrorHelperCore::OnNetErrorInfo(error_page::DnsProbeStatus status) {
  DCHECK_NE(error_page::DNS_PROBE_POSSIBLE, status);

  last_probe_status_ = status;

  if (!committed_error_page_info_ ||
      !committed_error_page_info_->needs_dns_updates ||
      !committed_error_page_info_->is_finished_loading) {
    return;
  }

  UpdateErrorPage();
}

void NetErrorHelperCore::OnSetCanShowNetworkDiagnosticsDialog(
    bool can_show_network_diagnostics_dialog) {
  can_show_network_diagnostics_dialog_ = can_show_network_diagnostics_dialog;
}

void NetErrorHelperCore::OnEasterEggHighScoreReceived(int high_score) {
  if (!committed_error_page_info_ ||
      !committed_error_page_info_->is_finished_loading) {
    return;
  }

  delegate_->InitializeErrorPageEasterEggHighScore(high_score);
}

void NetErrorHelperCore::PrepareErrorPageForMainFrame(
    ErrorPageInfo* pending_error_page_info,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  std::string error_param;
  error_page::Error error = pending_error_page_info->error;

  if (!alternative_error_page_info &&
      IsNetDnsError(pending_error_page_info->error)) {
    // The last probe status needs to be reset if this is a DNS error.  This
    // means that if a DNS error page is committed but has not yet finished
    // loading, a DNS probe status scheduled to be sent to it may be thrown
    // out, but since the new error page should trigger a new DNS probe, it
    // will just get the results for the next page load.
    last_probe_status_ = error_page::DNS_PROBE_POSSIBLE;
    pending_error_page_info->needs_dns_updates = true;
    error = GetUpdatedError(*pending_error_page_info);
  }
  if (error_html) {
    pending_error_page_info->page_state = delegate_->GenerateLocalizedErrorPage(
        error, pending_error_page_info->was_failed_post,
        can_show_network_diagnostics_dialog_,
        std::move(alternative_error_page_info), error_html);
  }
}

void NetErrorHelperCore::UpdateErrorPage() {
  DCHECK(committed_error_page_info_->needs_dns_updates);
  DCHECK(committed_error_page_info_->is_finished_loading);
  DCHECK_NE(error_page::DNS_PROBE_POSSIBLE, last_probe_status_);

  // Every status other than error_page::DNS_PROBE_POSSIBLE and
  // error_page::DNS_PROBE_STARTED is a final status code.  Once one is reached,
  // the page does not need further updates.
  if (last_probe_status_ != error_page::DNS_PROBE_STARTED) {
    committed_error_page_info_->needs_dns_updates = false;
    committed_error_page_info_->dns_probe_complete = true;
  }

  error_page::LocalizedError::PageState new_state =
      delegate_->UpdateErrorPage(GetUpdatedError(*committed_error_page_info_),
                                 committed_error_page_info_->was_failed_post,
                                 can_show_network_diagnostics_dialog_);

  committed_error_page_info_->page_state = std::move(new_state);
  if (!committed_error_page_info_->needs_dns_updates)
    ErrorPageLoadedWithFinalErrorCode();
}

error_page::Error NetErrorHelperCore::GetUpdatedError(
    const ErrorPageInfo& error_info) const {
  // If a probe didn't run or wasn't conclusive, restore the original error.
  const bool dns_probe_used =
      error_info.needs_dns_updates || error_info.dns_probe_complete;
  if (!dns_probe_used || last_probe_status_ == error_page::DNS_PROBE_NOT_RUN ||
      last_probe_status_ == error_page::DNS_PROBE_FINISHED_INCONCLUSIVE) {
    return error_info.error;
  }

  return error_page::Error::DnsProbeError(
      error_info.error.url(), last_probe_status_,
      error_info.error.stale_copy_in_cache());
}

void NetErrorHelperCore::Reload() {
  if (!committed_error_page_info_)
    return;
  delegate_->ReloadFrame();
}

#if BUILDFLAG(IS_ANDROID)
void NetErrorHelperCore::SetPageAutoFetcherHelperForTesting(
    std::unique_ptr<PageAutoFetcherHelper> page_auto_fetcher_helper) {
  page_auto_fetcher_helper_ = std::move(page_auto_fetcher_helper);
}
#endif

void NetErrorHelperCore::ExecuteButtonPress(Button button) {
  // If there's no committed error page, should not be invoked.
  DCHECK(committed_error_page_info_);

  switch (button) {
    case RELOAD_BUTTON:
      RecordEvent(error_page::NETWORK_ERROR_PAGE_RELOAD_BUTTON_CLICKED);
      navigation_from_button_ = RELOAD_BUTTON;
      Reload();
      return;
    case MORE_BUTTON:
      // Visual effects on page are handled in Javascript code.
      RecordEvent(error_page::NETWORK_ERROR_PAGE_MORE_BUTTON_CLICKED);
      return;
    case EASTER_EGG:
      RecordEvent(error_page::NETWORK_ERROR_EASTER_EGG_ACTIVATED);
      delegate_->RequestEasterEggHighScore();
      return;
    case DIAGNOSE_ERROR:
      RecordEvent(error_page::NETWORK_ERROR_DIAGNOSE_BUTTON_CLICKED);
      delegate_->DiagnoseError(committed_error_page_info_->error.url());
      return;
    case PORTAL_SIGNIN:
      RecordEvent(error_page::NETWORK_ERROR_PORTAL_SIGNIN_BUTTON_CLICKED);
      delegate_->PortalSignin();
      return;
    case DOWNLOAD_BUTTON:
      RecordEvent(error_page::NETWORK_ERROR_PAGE_DOWNLOAD_BUTTON_CLICKED);
      delegate_->DownloadPageLater();
      return;
    case NO_BUTTON:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void NetErrorHelperCore::LaunchOfflineItem(const std::string& id,
                                           const std::string& name_space) {
#if BUILDFLAG(IS_ANDROID)
  available_content_helper_.LaunchItem(id, name_space);
#endif
}

void NetErrorHelperCore::LaunchDownloadsPage() {
#if BUILDFLAG(IS_ANDROID)
  available_content_helper_.LaunchDownloadsPage();
#endif
}

void NetErrorHelperCore::SavePageForLater() {
#if BUILDFLAG(IS_ANDROID)
  page_auto_fetcher_helper_->TrySchedule(
      /*user_requested=*/true, base::BindOnce(&Delegate::SetAutoFetchState,
                                              base::Unretained(delegate_)));
#endif
}

void NetErrorHelperCore::CancelSavePage() {
#if BUILDFLAG(IS_ANDROID)
  page_auto_fetcher_helper_->CancelSchedule();
#endif
}

void NetErrorHelperCore::ListVisibilityChanged(bool is_visible) {
#if BUILDFLAG(IS_ANDROID)
  available_content_helper_.ListVisibilityChanged(is_visible);
#endif
}
