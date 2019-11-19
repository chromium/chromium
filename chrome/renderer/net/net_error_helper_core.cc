// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper_core.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/localized_error.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_thread.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/web_string.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

// |NetErrorNavigationCorrectionTypes| enum id for Web search query.
// Other correction types uses the |kCorrectionResourceTable| array order.
const int kWebSearchQueryUMAId = 100;

// Number of URL correction suggestions to display.
const int kMaxUrlCorrectionsToDisplay = 1;

struct CorrectionTypeToResourceTable {
  int resource_id;
  const char* correction_type;
};

// Note: Ordering should be the same as |NetErrorNavigationCorrectionTypes| enum
// in histograms.xml.
const CorrectionTypeToResourceTable kCorrectionResourceTable[] = {
    {IDS_ERRORPAGES_SUGGESTION_VISIT_GOOGLE_CACHE, "cachedPage"},
    // "reloadPage" is has special handling.
    {IDS_ERRORPAGES_SUGGESTION_CORRECTED_URL, "urlCorrection"},
    {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "siteDomain"},
    {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "host"},
    {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "sitemap"},
    {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "pathParentFolder"},
    // "siteSearchQuery" is not yet supported.
    // TODO(mmenke):  Figure out what format "siteSearchQuery" uses for its
    // suggestions.
    // "webSearchQuery" has special handling.
    {IDS_ERRORPAGES_SUGGESTION_ALTERNATE_URL, "contentOverlap"},
    {IDS_ERRORPAGES_SUGGESTION_CORRECTED_URL, "emphasizedUrlCorrection"},
};

struct NavigationCorrection {
  NavigationCorrection() : is_porn(false), is_soft_porn(false) {}

  static void RegisterJSONConverter(
      base::JSONValueConverter<NavigationCorrection>* converter) {
    converter->RegisterStringField("correctionType",
                                   &NavigationCorrection::correction_type);
    converter->RegisterStringField("urlCorrection",
                                   &NavigationCorrection::url_correction);
    converter->RegisterStringField("clickType",
                                   &NavigationCorrection::click_type);
    converter->RegisterStringField("clickData",
                                   &NavigationCorrection::click_data);
    converter->RegisterBoolField("isPorn", &NavigationCorrection::is_porn);
    converter->RegisterBoolField("isSoftPorn",
                                 &NavigationCorrection::is_soft_porn);
  }

  std::string correction_type;
  std::string url_correction;
  std::string click_type;
  std::string click_data;
  bool is_porn;
  bool is_soft_porn;
};

struct NavigationCorrectionResponse {
  std::string event_id;
  std::string fingerprint;
  std::vector<std::unique_ptr<NavigationCorrection>> corrections;

  static void RegisterJSONConverter(
      base::JSONValueConverter<NavigationCorrectionResponse>* converter) {
    converter->RegisterStringField("result.eventId",
                                   &NavigationCorrectionResponse::event_id);
    converter->RegisterStringField("result.fingerprint",
                                   &NavigationCorrectionResponse::fingerprint);
    converter->RegisterRepeatedMessage(
        "result.UrlCorrections", &NavigationCorrectionResponse::corrections);
  }
};

base::TimeDelta GetAutoReloadTime(size_t reload_count) {
  static const int kDelaysMs[] = {0,      5000,   30000,  60000,
                                  300000, 600000, 1800000};
  if (reload_count >= base::size(kDelaysMs))
    reload_count = base::size(kDelaysMs) - 1;
  return base::TimeDelta::FromMilliseconds(kDelaysMs[reload_count]);
}

// Returns whether |error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it).
bool IsNetDnsError(const error_page::Error& error) {
  return error.domain() == error_page::Error::kNetErrorDomain &&
         net::IsDnsError(error.reason());
}

GURL SanitizeURL(const GURL& url) {
  GURL::Replacements remove_params;
  remove_params.ClearUsername();
  remove_params.ClearPassword();
  remove_params.ClearQuery();
  remove_params.ClearRef();
  return url.ReplaceComponents(remove_params);
}

// Sanitizes and formats a URL for upload to the error correction service.
std::string PrepareUrlForUpload(const GURL& url) {
  // TODO(yuusuke): Change to url_formatter::FormatUrl when Link Doctor becomes
  // unicode-capable.
  std::string spec_to_send = SanitizeURL(url).spec();

  // Notify navigation correction service of the url truncation by sending of
  // "?" at the end.
  if (url.has_query())
    spec_to_send.append("?");
  return spec_to_send;
}

// Given an Error, returns true if the FixURL service should be used
// for that error.  Also sets |error_param| to the string that should be sent to
// the FixURL service to identify the error type.
bool ShouldUseFixUrlServiceForError(const error_page::Error& error,
                                    std::string* error_param) {
  error_param->clear();

  // Don't use the correction service for HTTPS (for privacy reasons).
  GURL unreachable_url(error.url());
  if (GURL(unreachable_url).SchemeIsCryptographic())
    return false;

  const auto& domain = error.domain();
  if (domain == error_page::Error::kHttpErrorDomain && error.reason() == 404) {
    *error_param = "http404";
    return true;
  }
  if (IsNetDnsError(error)) {
    *error_param = "dnserror";
    return true;
  }
  if (domain == error_page::Error::kNetErrorDomain &&
      (error.reason() == net::ERR_CONNECTION_FAILED ||
       error.reason() == net::ERR_CONNECTION_REFUSED ||
       error.reason() == net::ERR_ADDRESS_UNREACHABLE ||
       error.reason() == net::ERR_CONNECTION_TIMED_OUT)) {
    *error_param = "connectionFailure";
    return true;
  }
  return false;
}

// Creates a request body for use with the fixurl service.  Sets parameters
// shared by all types of requests to the service.  |correction_params| must
// contain the parameters specific to the actual request type.
std::string CreateRequestBody(
    const std::string& method,
    const std::string& error_param,
    const NetErrorHelperCore::NavigationCorrectionParams& correction_params,
    std::unique_ptr<base::DictionaryValue> params_dict) {
  // Set params common to all request types.
  params_dict->SetString("key", correction_params.api_key);
  params_dict->SetString("clientName", "chrome");
  params_dict->SetString("error", error_param);

  if (!correction_params.language.empty())
    params_dict->SetString("language", correction_params.language);

  if (!correction_params.country_code.empty())
    params_dict->SetString("originCountry", correction_params.country_code);

  base::DictionaryValue request_dict;
  request_dict.SetString("method", method);
  request_dict.SetString("apiVersion", "v1");
  request_dict.Set("params", std::move(params_dict));

  std::string request_body;
  bool success = base::JSONWriter::Write(request_dict, &request_body);
  DCHECK(success);
  return request_body;
}

// If URL correction information should be retrieved remotely for a main frame
// load that failed with |error|, returns true and sets
// |correction_request_body| to be the body for the correction request.
std::string CreateFixUrlRequestBody(
    const error_page::Error& error,
    const NetErrorHelperCore::NavigationCorrectionParams& correction_params) {
  std::string error_param;
  bool result = ShouldUseFixUrlServiceForError(error, &error_param);
  DCHECK(result);

  // TODO(mmenke):  Investigate open sourcing the relevant protocol buffers and
  //                using those directly instead.
  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetString("urlQuery", PrepareUrlForUpload(error.url()));
  return CreateRequestBody("linkdoctor.fixurl.fixurl", error_param,
                           correction_params, std::move(params));
}

std::string CreateClickTrackingUrlRequestBody(
    const error_page::Error& error,
    const NetErrorHelperCore::NavigationCorrectionParams& correction_params,
    const NavigationCorrectionResponse& response,
    const NavigationCorrection& correction) {
  std::string error_param;
  bool result = ShouldUseFixUrlServiceForError(error, &error_param);
  DCHECK(result);

  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());

  params->SetString("originalUrlQuery", PrepareUrlForUpload(error.url()));

  params->SetString("clickedUrlCorrection", correction.url_correction);
  params->SetString("clickType", correction.click_type);
  params->SetString("clickData", correction.click_data);

  params->SetString("eventId", response.event_id);
  params->SetString("fingerprint", response.fingerprint);

  return CreateRequestBody("linkdoctor.fixurl.clicktracking", error_param,
                           correction_params, std::move(params));
}

base::string16 FormatURLForDisplay(const GURL& url, bool is_rtl) {
  // Translate punycode into UTF8, unescape UTF8 URLs.
  base::string16 url_for_display(url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitNothing, net::UnescapeRule::NORMAL,
      nullptr, nullptr, nullptr));
  // URLs are always LTR.
  if (is_rtl)
    base::i18n::WrapStringWithLTRFormatting(&url_for_display);
  return url_for_display;
}

std::unique_ptr<NavigationCorrectionResponse> ParseNavigationCorrectionResponse(
    const std::string raw_response) {
  // TODO(mmenke):  Open source related protocol buffers and use them directly.
  std::unique_ptr<base::Value> parsed =
      base::JSONReader::ReadDeprecated(raw_response);
  std::unique_ptr<NavigationCorrectionResponse> response(
      new NavigationCorrectionResponse());
  base::JSONValueConverter<NavigationCorrectionResponse> converter;
  if (!parsed || !converter.Convert(*parsed, response.get()))
    response.reset();
  return response;
}

void LogCorrectionTypeShown(int type_id) {
  UMA_HISTOGRAM_ENUMERATION(
      "Net.ErrorPageCounts.NavigationCorrectionLinksShown", type_id,
      kWebSearchQueryUMAId + 1);
}

std::unique_ptr<error_page::ErrorPageParams> CreateErrorPageParams(
    const NavigationCorrectionResponse& response,
    const error_page::Error& error,
    const NetErrorHelperCore::NavigationCorrectionParams& correction_params,
    bool is_rtl) {
  // Version of URL for display in suggestions.  It has to be sanitized first
  // because any received suggestions will be relative to the sanitized URL.
  base::string16 original_url_for_display =
      FormatURLForDisplay(SanitizeURL(GURL(error.url())), is_rtl);

  std::unique_ptr<error_page::ErrorPageParams> params(
      new error_page::ErrorPageParams());
  params->override_suggestions.reset(new base::ListValue());
  std::unique_ptr<base::ListValue> parsed_corrections(new base::ListValue());
  for (auto it = response.corrections.begin(); it != response.corrections.end();
       ++it) {
    // Doesn't seem like a good idea to show these.
    if ((*it)->is_porn || (*it)->is_soft_porn)
      continue;

    int tracking_id = it - response.corrections.begin();

    if ((*it)->correction_type == "reloadPage") {
      params->suggest_reload = true;
      params->reload_tracking_id = tracking_id;
      continue;
    }

    if ((*it)->correction_type == "webSearchQuery") {
      // If there are multiple searches suggested, use the first suggestion.
      if (params->search_terms.empty()) {
        params->search_url = correction_params.search_url;
        params->search_terms = (*it)->url_correction;
        params->search_tracking_id = tracking_id;
        LogCorrectionTypeShown(kWebSearchQueryUMAId);
      }
      continue;
    }

    // Allow reload page and web search query to be empty strings, but not
    // links.
    if ((*it)->url_correction.empty() ||
        (params->override_suggestions->GetSize() >=
         kMaxUrlCorrectionsToDisplay)) {
      continue;
    }

    size_t correction_index;
    for (correction_index = 0;
         correction_index < base::size(kCorrectionResourceTable);
         ++correction_index) {
      if ((*it)->correction_type !=
          kCorrectionResourceTable[correction_index].correction_type) {
        continue;
      }
      std::unique_ptr<base::DictionaryValue> suggest(
          new base::DictionaryValue());
      suggest->SetString(
          "summary",
          l10n_util::GetStringUTF16(
              kCorrectionResourceTable[correction_index].resource_id));
      suggest->SetString("urlCorrection", (*it)->url_correction);
      suggest->SetString(
          "urlCorrectionForDisplay",
          FormatURLForDisplay(GURL((*it)->url_correction), is_rtl));
      suggest->SetString("originalUrlForDisplay", original_url_for_display);
      suggest->SetInteger("trackingId", tracking_id);
      suggest->SetInteger("type", static_cast<int>(correction_index));

      params->override_suggestions->Append(std::move(suggest));
      LogCorrectionTypeShown(static_cast<int>(correction_index));
      break;
    }
  }

  if (params->override_suggestions->empty() && !params->search_url.is_valid())
    params.reset();
  return params;
}

// Tracks navigation correction service usage in UMA to enable more in depth
// analysis.
void TrackClickUMA(std::string type_id) {
  // Web search suggestion isn't in |kCorrectionResourceTable| array.
  if (type_id == "webSearchQuery") {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.ErrorPageCounts.NavigationCorrectionLinksUsed",
        kWebSearchQueryUMAId, kWebSearchQueryUMAId + 1);
    return;
  }

  size_t correction_index;
  for (correction_index = 0;
       correction_index < base::size(kCorrectionResourceTable);
       ++correction_index) {
    if (kCorrectionResourceTable[correction_index].correction_type == type_id) {
      UMA_HISTOGRAM_ENUMERATION(
          "Net.ErrorPageCounts.NavigationCorrectionLinksUsed",
          static_cast<int>(correction_index), kWebSearchQueryUMAId + 1);
      break;
    }
  }
}

}  // namespace

struct NetErrorHelperCore::ErrorPageInfo {
  ErrorPageInfo(error_page::Error error, bool was_failed_post)
      : error(error),
        was_failed_post(was_failed_post),
        needs_dns_updates(false),
        needs_load_navigation_corrections(false),
        is_finished_loading(false),
        auto_reload_triggered(false) {}

  // Information about the failed page load.
  error_page::Error error;
  bool was_failed_post;

  // Information about the status of the error page.

  // True if a page is a DNS error page and has not yet received a final DNS
  // probe status.
  bool needs_dns_updates;
  bool dns_probe_complete = false;

  // True if a blank page was loaded, and navigation corrections need to be
  // loaded to generate the real error page.
  bool needs_load_navigation_corrections;

  // Navigation correction service paramers, which will be used in response to
  // certain types of network errors.  They are all stored here in case they
  // change over the course of displaying the error page.
  std::unique_ptr<NetErrorHelperCore::NavigationCorrectionParams>
      navigation_correction_params;

  std::unique_ptr<NavigationCorrectionResponse> navigation_correction_response;

  // All the navigation corrections that have been clicked, for tracking
  // purposes.
  std::set<int> clicked_corrections;

  // True if a page has completed loading, at which point it can receive
  // updates.
  bool is_finished_loading;

  // True if the auto-reload timer has fired and a reload is or has been in
  // flight.
  bool auto_reload_triggered;

  error_page::LocalizedError::PageState page_state;
};

NetErrorHelperCore::NavigationCorrectionParams::NavigationCorrectionParams() {}

NetErrorHelperCore::NavigationCorrectionParams::NavigationCorrectionParams(
    const NavigationCorrectionParams& other) = default;

NetErrorHelperCore::NavigationCorrectionParams::~NavigationCorrectionParams() {}

bool NetErrorHelperCore::IsReloadableError(
    const NetErrorHelperCore::ErrorPageInfo& info) {
  GURL url = info.error.url();
  return info.error.domain() == error_page::Error::kNetErrorDomain &&
         info.error.reason() != net::ERR_ABORTED &&
         // For now, net::ERR_UNKNOWN_URL_SCHEME is only being displayed on
         // Chrome for Android.
         info.error.reason() != net::ERR_UNKNOWN_URL_SCHEME &&
         // Do not trigger if the server rejects a client certificate.
         // https://crbug.com/431387
         !net::IsClientCertificateError(info.error.reason()) &&
         // Some servers reject client certificates with a generic
         // handshake_failure alert.
         // https://crbug.com/431387
         info.error.reason() != net::ERR_SSL_PROTOCOL_ERROR &&
         // Do not trigger for blacklisted URLs.
         // https://crbug.com/803839
         info.error.reason() != net::ERR_BLOCKED_BY_ADMINISTRATOR &&
         // Do not trigger for requests that were blocked by the browser itself.
         info.error.reason() != net::ERR_BLOCKED_BY_CLIENT &&
         !info.was_failed_post &&
         // Do not trigger for this error code because it is used by Chrome
         // while an auth prompt is being displayed.
         info.error.reason() != net::ERR_INVALID_AUTH_CREDENTIALS &&
         // Don't auto-reload non-http/https schemas.
         // https://crbug.com/471713
         url.SchemeIsHTTPOrHTTPS();
}

NetErrorHelperCore::NetErrorHelperCore(Delegate* delegate,
                                       bool auto_reload_enabled,
                                       bool is_visible)
    : delegate_(delegate),
      last_probe_status_(error_page::DNS_PROBE_POSSIBLE),
      can_show_network_diagnostics_dialog_(false),
      auto_reload_enabled_(auto_reload_enabled),
      auto_reload_timer_(new base::OneShotTimer()),
      auto_reload_paused_(false),
      auto_reload_in_flight_(false),
      uncommitted_load_started_(false),
      online_(content::RenderThread::Get()->IsOnline()),
      visible_(is_visible),
      auto_reload_count_(0),
      navigation_from_button_(NO_BUTTON)
#if defined(OS_ANDROID)
      ,
      page_auto_fetcher_helper_(
          std::make_unique<PageAutoFetcherHelper>(delegate->GetRenderFrame()))
#endif
{
}

NetErrorHelperCore::~NetErrorHelperCore() = default;

void NetErrorHelperCore::CancelPendingFetches() {
  // Cancel loading the alternate error page, and prevent any pending error page
  // load from starting a new error page load.  Swapping in the error page when
  // it's finished loading could abort the navigation, otherwise.
  if (committed_error_page_info_)
    committed_error_page_info_->needs_load_navigation_corrections = false;
  if (pending_error_page_info_)
    pending_error_page_info_->needs_load_navigation_corrections = false;
  delegate_->CancelFetchNavigationCorrections();
  auto_reload_timer_->Stop();
  auto_reload_paused_ = false;
}

void NetErrorHelperCore::OnStop() {
  CancelPendingFetches();
  uncommitted_load_started_ = false;
  auto_reload_count_ = 0;
  auto_reload_in_flight_ = false;
}

void NetErrorHelperCore::OnWasShown() {
  visible_ = true;
  if (auto_reload_paused_)
    MaybeStartAutoReloadTimer();
}

void NetErrorHelperCore::OnWasHidden() {
  visible_ = false;
  PauseAutoReloadTimer();
}

void NetErrorHelperCore::OnStartLoad(FrameType frame_type, PageType page_type) {
  if (frame_type != MAIN_FRAME)
    return;

  uncommitted_load_started_ = true;

  // If there's no pending error page information associated with the page load,
  // or the new page is not an error page, then reset pending error page state.
  if (!pending_error_page_info_ || page_type != ERROR_PAGE) {
    CancelPendingFetches();
  } else {
    // Halt auto-reload if it's currently scheduled. OnFinishLoad will trigger
    // auto-reload if appropriate.
    PauseAutoReloadTimer();
  }
}

void NetErrorHelperCore::OnCommitLoad(FrameType frame_type, const GURL& url) {
  if (frame_type != MAIN_FRAME)
    return;

  // If a page is committing, either it's an error page and autoreload will be
  // started again below, or it's a success page and we need to clear autoreload
  // state.
  auto_reload_in_flight_ = false;

  // uncommitted_load_started_ could already be false, since RenderFrameImpl
  // calls OnCommitLoad once for each in-page navigation (like a fragment
  // change) with no corresponding OnStartLoad.
  uncommitted_load_started_ = false;

#if defined(OS_ANDROID)
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

#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)

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

  if (!committed_error_page_info_) {
    auto_reload_count_ = 0;
    return;
  }
  committed_error_page_info_->is_finished_loading = true;

  RecordEvent(error_page::NETWORK_ERROR_PAGE_SHOWN);
  if (committed_error_page_info_->page_state.show_cached_copy_button_shown) {
    RecordEvent(error_page::NETWORK_ERROR_PAGE_CACHED_COPY_BUTTON_SHOWN);
  }

  delegate_->SetIsShowingDownloadButton(
      committed_error_page_info_->page_state.download_button_shown);

  delegate_->EnablePageHelperFunctions();

  if (committed_error_page_info_->needs_load_navigation_corrections) {
    // If there is another pending error page load, |fix_url| should have been
    // cleared.
    DCHECK(!pending_error_page_info_);
    DCHECK(!committed_error_page_info_->needs_dns_updates);
    delegate_->FetchNavigationCorrections(
        committed_error_page_info_->navigation_correction_params->url,
        CreateFixUrlRequestBody(
            committed_error_page_info_->error,
            *committed_error_page_info_->navigation_correction_params));
  } else if (auto_reload_enabled_ &&
             IsReloadableError(*committed_error_page_info_)) {
    MaybeStartAutoReloadTimer();
  }

  DVLOG(1) << "Error page finished loading; sending saved status.";
  if (committed_error_page_info_->needs_dns_updates) {
    if (last_probe_status_ != error_page::DNS_PROBE_POSSIBLE)
      UpdateErrorPage();
  } else {
    ErrorPageLoadedWithFinalErrorCode();
  }
}

void NetErrorHelperCore::PrepareErrorPage(FrameType frame_type,
                                          const error_page::Error& error,
                                          bool is_failed_post,
                                          std::string* error_html) {
  if (frame_type == MAIN_FRAME) {
    // If navigation corrections were needed before, that should have been
    // cancelled earlier by starting a new page load (Which has now failed).
    DCHECK(!committed_error_page_info_ ||
           !committed_error_page_info_->needs_load_navigation_corrections);

    pending_error_page_info_.reset(new ErrorPageInfo(error, is_failed_post));
    pending_error_page_info_->navigation_correction_params.reset(
        new NavigationCorrectionParams(navigation_correction_params_));
    PrepareErrorPageForMainFrame(pending_error_page_info_.get(), error_html);
  } else {
    if (error_html) {
      delegate_->GenerateLocalizedErrorPage(
          error, is_failed_post,
          false /* No diagnostics dialogs allowed for subframes. */, nullptr,
          error_html);
    }
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

void NetErrorHelperCore::OnSetNavigationCorrectionInfo(
    const GURL& navigation_correction_url,
    const std::string& language,
    const std::string& country_code,
    const std::string& api_key,
    const GURL& search_url) {
  navigation_correction_params_.url = navigation_correction_url;
  navigation_correction_params_.language = language;
  navigation_correction_params_.country_code = country_code;
  navigation_correction_params_.api_key = api_key;
  navigation_correction_params_.search_url = search_url;
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
    std::string* error_html) {
  std::string error_param;
  error_page::Error error = pending_error_page_info->error;

  if (pending_error_page_info->navigation_correction_params &&
      pending_error_page_info->navigation_correction_params->url.is_valid() &&
      ShouldUseFixUrlServiceForError(error, &error_param)) {
    pending_error_page_info->needs_load_navigation_corrections = true;
    return;
  }

  if (IsNetDnsError(pending_error_page_info->error)) {
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
        can_show_network_diagnostics_dialog_, nullptr, error_html);
  }
}

void NetErrorHelperCore::UpdateErrorPage() {
  DCHECK(committed_error_page_info_->needs_dns_updates);
  DCHECK(committed_error_page_info_->is_finished_loading);
  DCHECK_NE(error_page::DNS_PROBE_POSSIBLE, last_probe_status_);

  UMA_HISTOGRAM_ENUMERATION("DnsProbe.ErrorPageUpdateStatus",
                            last_probe_status_, error_page::DNS_PROBE_MAX);
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

  // This button can't be changed by a DNS error update, so there's no code
  // to update the related UMA in ErrorPageLoadedWithFinalErrorCode(). Instead,
  // verify there's no change in this button's state.
  DCHECK_EQ(
      committed_error_page_info_->page_state.show_cached_copy_button_shown,
      new_state.show_cached_copy_button_shown);

  committed_error_page_info_->page_state = std::move(new_state);
  if (!committed_error_page_info_->needs_dns_updates)
    ErrorPageLoadedWithFinalErrorCode();
}

void NetErrorHelperCore::OnNavigationCorrectionsFetched(
    const std::string& corrections,
    bool is_rtl) {
  // Loading suggestions only starts when a blank error page finishes loading,
  // and is cancelled with a new load.
  DCHECK(!pending_error_page_info_);
  DCHECK(committed_error_page_info_->is_finished_loading);
  DCHECK(committed_error_page_info_->needs_load_navigation_corrections);
  DCHECK(committed_error_page_info_->navigation_correction_params);

  pending_error_page_info_.reset(
      new ErrorPageInfo(committed_error_page_info_->error,
                        committed_error_page_info_->was_failed_post));
  pending_error_page_info_->navigation_correction_response =
      ParseNavigationCorrectionResponse(corrections);

  std::string error_html;
  if (pending_error_page_info_->navigation_correction_response) {
    // Copy navigation correction parameters used for the request, so tracking
    // requests can still be sent if the configuration changes.
    pending_error_page_info_->navigation_correction_params.reset(
        new NavigationCorrectionParams(
            *committed_error_page_info_->navigation_correction_params));
    std::unique_ptr<error_page::ErrorPageParams> params = CreateErrorPageParams(
        *pending_error_page_info_->navigation_correction_response,
        pending_error_page_info_->error,
        *pending_error_page_info_->navigation_correction_params, is_rtl);
    pending_error_page_info_->page_state =
        delegate_->GenerateLocalizedErrorPage(
            pending_error_page_info_->error,
            pending_error_page_info_->was_failed_post,
            can_show_network_diagnostics_dialog_, std::move(params),
            &error_html);
  } else {
    // Since |navigation_correction_params| in |pending_error_page_info_| is
    // NULL, this won't trigger another attempt to load corrections.
    PrepareErrorPageForMainFrame(pending_error_page_info_.get(), &error_html);
  }

  // TODO(mmenke):  Once the new API is in place, look into replacing this
  //                double page load by just updating the error page, like DNS
  //                probes do.
  delegate_->LoadErrorPage(error_html, pending_error_page_info_->error.url());
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

bool NetErrorHelperCore::MaybeStartAutoReloadTimer() {
  // Automation tools expect to be in control of reloads.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomation)) {
    return false;
  }

  if (!committed_error_page_info_ ||
      !committed_error_page_info_->is_finished_loading ||
      pending_error_page_info_ || uncommitted_load_started_) {
    return false;
  }

  StartAutoReloadTimer();
  return true;
}

void NetErrorHelperCore::StartAutoReloadTimer() {
  DCHECK(committed_error_page_info_);
  DCHECK(IsReloadableError(*committed_error_page_info_));

  committed_error_page_info_->auto_reload_triggered = true;

  if (!online_ || !visible_) {
    auto_reload_paused_ = true;
    return;
  }

  auto_reload_paused_ = false;
  base::TimeDelta delay = GetAutoReloadTime(auto_reload_count_);
  auto_reload_timer_->Stop();
  auto_reload_timer_->Start(
      FROM_HERE, delay,
      base::Bind(&NetErrorHelperCore::AutoReloadTimerFired,
                 base::Unretained(this)));
}

void NetErrorHelperCore::AutoReloadTimerFired() {
  // AutoReloadTimerFired only runs if:
  // 1. StartAutoReloadTimer was previously called, which requires that
  //    committed_error_page_info_ is populated;
  // 2. No other page load has started since (1), since OnStartLoad stops the
  //    auto-reload timer.
  DCHECK(committed_error_page_info_);

  auto_reload_count_++;
  auto_reload_in_flight_ = true;
  Reload();
}

void NetErrorHelperCore::PauseAutoReloadTimer() {
  if (!auto_reload_timer_->IsRunning())
    return;
  DCHECK(committed_error_page_info_);
  DCHECK(!auto_reload_paused_);
  DCHECK(committed_error_page_info_->auto_reload_triggered);
  auto_reload_timer_->Stop();
  auto_reload_paused_ = true;
}

void NetErrorHelperCore::NetworkStateChanged(bool online) {
  bool was_online = online_;
  online_ = online;
  if (!was_online && online) {
    // Transitioning offline -> online
    if (auto_reload_paused_)
      MaybeStartAutoReloadTimer();
  } else if (was_online && !online) {
    // Transitioning online -> offline
    if (auto_reload_timer_->IsRunning())
      auto_reload_count_ = 0;
    PauseAutoReloadTimer();
  }
}

bool NetErrorHelperCore::ShouldSuppressErrorPage(FrameType frame_type,
                                                 const GURL& url) {
  // Don't suppress child frame errors.
  if (frame_type != MAIN_FRAME)
    return false;

  // If there's no auto reload attempt in flight, this error page didn't come
  // from auto reload, so don't suppress it.
  if (!auto_reload_in_flight_)
    return false;

  uncommitted_load_started_ = false;
  // This serves to terminate the auto-reload in flight attempt. If
  // ShouldSuppressErrorPage is called, the auto-reload yielded an error, which
  // means the request was already sent.
  auto_reload_in_flight_ = false;
  MaybeStartAutoReloadTimer();
  return true;
}

#if defined(OS_ANDROID)
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
    case SHOW_CACHED_COPY_BUTTON:
      RecordEvent(error_page::NETWORK_ERROR_PAGE_CACHED_COPY_BUTTON_CLICKED);
      return;
    case DIAGNOSE_ERROR:
      RecordEvent(error_page::NETWORK_ERROR_DIAGNOSE_BUTTON_CLICKED);
      delegate_->DiagnoseError(committed_error_page_info_->error.url());
      return;
    case DOWNLOAD_BUTTON:
      RecordEvent(error_page::NETWORK_ERROR_PAGE_DOWNLOAD_BUTTON_CLICKED);
      delegate_->DownloadPageLater();
      return;
    case NO_BUTTON:
      NOTREACHED();
      return;
  }
}

void NetErrorHelperCore::TrackClick(int tracking_id) {
  // It's technically possible for |navigation_correction_params| to be NULL but
  // for |navigation_correction_response| not to be NULL, if the paramters
  // changed between loading the original error page and loading the error page
  if (!committed_error_page_info_ ||
      !committed_error_page_info_->navigation_correction_response) {
    return;
  }

  NavigationCorrectionResponse* response =
      committed_error_page_info_->navigation_correction_response.get();

  // |tracking_id| is less than 0 when the error page was not generated by the
  // navigation correction service.  |tracking_id| should never be greater than
  // the array size, but best to be safe, since it contains data from a remote
  // site, though none of that data should make it into Javascript callbacks.
  if (tracking_id < 0 ||
      static_cast<size_t>(tracking_id) >= response->corrections.size()) {
    return;
  }

  // Only report a clicked link once.
  if (committed_error_page_info_->clicked_corrections.count(tracking_id))
    return;

  TrackClickUMA(response->corrections[tracking_id]->correction_type);

  committed_error_page_info_->clicked_corrections.insert(tracking_id);
  std::string request_body = CreateClickTrackingUrlRequestBody(
      committed_error_page_info_->error,
      *committed_error_page_info_->navigation_correction_params, *response,
      *response->corrections[tracking_id]);
  delegate_->SendTrackingRequest(
      committed_error_page_info_->navigation_correction_params->url,
      request_body);
}

void NetErrorHelperCore::LaunchOfflineItem(const std::string& id,
                                           const std::string& name_space) {
#if defined(OS_ANDROID)
  available_content_helper_.LaunchItem(id, name_space);
#endif
}

void NetErrorHelperCore::LaunchDownloadsPage() {
#if defined(OS_ANDROID)
  available_content_helper_.LaunchDownloadsPage();
#endif
}

void NetErrorHelperCore::SavePageForLater() {
#if defined(OS_ANDROID)
  page_auto_fetcher_helper_->TrySchedule(
      /*user_requested=*/true, base::BindOnce(&Delegate::SetAutoFetchState,
                                              base::Unretained(delegate_)));
#endif
}

void NetErrorHelperCore::CancelSavePage() {
#if defined(OS_ANDROID)
  page_auto_fetcher_helper_->CancelSchedule();
#endif
}

void NetErrorHelperCore::ListVisibilityChanged(bool is_visible) {
#if defined(OS_ANDROID)
  available_content_helper_.ListVisibilityChanged(is_visible);
#endif
}
