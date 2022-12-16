// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_event_type.h"
#include "services/network/public/cpp/request_destination.h"

using security_interstitials::UnsafeResource;

namespace safe_browsing {
namespace {

// Maximum time in milliseconds to wait for the SafeBrowsing service reputation
// check. After this amount of time the outstanding check will be aborted, and
// the resource will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;

constexpr char kMatchResultHistogramName[] =
    "SafeBrowsing.RT.LocalMatch.Result";

void RecordCheckUrlTimeout(bool timed_out) {
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.CheckUrl.Timeout", timed_out);
}

void RecordLocalMatchResult(
    bool has_match,
    network::mojom::RequestDestination request_destination,
    std::string url_lookup_service_metric_suffix) {
  AsyncMatch match_result =
      has_match ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH;
  base::UmaHistogramEnumeration(kMatchResultHistogramName, match_result);
  bool is_mainframe =
      request_destination == network::mojom::RequestDestination::kDocument;
  std::string frame_suffix = is_mainframe ? ".Mainframe" : ".NonMainframe";
  base::UmaHistogramEnumeration(kMatchResultHistogramName + frame_suffix,
                                match_result);
  base::UmaHistogramEnumeration(kMatchResultHistogramName + frame_suffix +
                                    url_lookup_service_metric_suffix,
                                match_result);
}

}  // namespace

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(CheckUrlCallback callback)
    : callback_(std::move(callback)) {}

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(
    NativeCheckUrlCallback native_callback)
    : native_callback_(std::move(native_callback)) {}

SafeBrowsingUrlCheckerImpl::Notifier::~Notifier() = default;

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(Notifier&& other) = default;

SafeBrowsingUrlCheckerImpl::Notifier& SafeBrowsingUrlCheckerImpl::Notifier::
operator=(Notifier&& other) = default;

void SafeBrowsingUrlCheckerImpl::Notifier::OnStartSlowCheck() {
  if (callback_) {
    std::move(callback_).Run(slow_check_notifier_.BindNewPipeAndPassReceiver(),
                             false, false, false, false);
    return;
  }

  DCHECK(native_callback_);
  std::move(native_callback_)
      .Run(&native_slow_check_notifier_, false, false, false, false);
}

void SafeBrowsingUrlCheckerImpl::Notifier::OnCompleteCheck(
    bool proceed,
    bool showed_interstitial,
    bool did_perform_real_time_check,
    bool did_check_allowlist) {
  if (callback_) {
    std::move(callback_).Run(mojo::NullReceiver(), proceed, showed_interstitial,
                             did_perform_real_time_check, did_check_allowlist);
    return;
  }

  if (native_callback_) {
    std::move(native_callback_)
        .Run(nullptr, proceed, showed_interstitial, did_perform_real_time_check,
             did_check_allowlist);
    return;
  }

  if (slow_check_notifier_) {
    slow_check_notifier_->OnCompleteCheck(proceed, showed_interstitial,
                                          did_perform_real_time_check,
                                          did_check_allowlist);
    slow_check_notifier_.reset();
    return;
  }

  std::move(native_slow_check_notifier_)
      .Run(proceed, showed_interstitial, did_perform_real_time_check,
           did_check_allowlist);
}

SafeBrowsingUrlCheckerImpl::UrlInfo::UrlInfo(const GURL& in_url,
                                             const std::string& in_method,
                                             Notifier in_notifier,
                                             bool in_is_cached_safe_url,
                                             bool did_perform_real_time_check,
                                             bool did_check_allowlist)
    : url(in_url),
      method(in_method),
      notifier(std::move(in_notifier)),
      is_cached_safe_url(in_is_cached_safe_url),
      did_perform_real_time_check(did_perform_real_time_check),
      did_check_allowlist(did_check_allowlist) {}

SafeBrowsingUrlCheckerImpl::UrlInfo::UrlInfo(UrlInfo&& other) = default;

SafeBrowsingUrlCheckerImpl::UrlInfo::~UrlInfo() = default;

SafeBrowsingUrlCheckerImpl::SafeBrowsingUrlCheckerImpl(
    const net::HttpRequestHeaders& headers,
    int load_flags,
    network::mojom::RequestDestination request_destination,
    bool has_user_gesture,
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    UnsafeResource::RenderProcessId render_process_id,
    UnsafeResource::RenderFrameId render_frame_id,
    UnsafeResource::FrameTreeNodeId frame_tree_node_id,
    bool real_time_lookup_enabled,
    bool can_rt_check_subresource_url,
    bool can_check_db,
    bool can_check_high_confidence_allowlist,
    std::string url_lookup_service_metric_suffix,
    GURL last_committed_url,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    WebUIDelegate* webui_delegate)
    : headers_(headers),
      load_flags_(load_flags),
      request_destination_(request_destination),
      has_user_gesture_(has_user_gesture),
      web_contents_getter_(web_contents_getter),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      frame_tree_node_id_(frame_tree_node_id),
      url_checker_delegate_(std::move(url_checker_delegate)),
      database_manager_(url_checker_delegate_->GetDatabaseManager()),
      real_time_lookup_enabled_(real_time_lookup_enabled),
      can_rt_check_subresource_url_(can_rt_check_subresource_url),
      can_check_db_(can_check_db),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      last_committed_url_(last_committed_url),
      ui_task_runner_(ui_task_runner),
      url_lookup_service_on_ui_(url_lookup_service_on_ui),
      webui_delegate_(webui_delegate) {
  DCHECK(!web_contents_getter_.is_null());
  DCHECK(!can_rt_check_subresource_url_ || real_time_lookup_enabled_);
  DCHECK(real_time_lookup_enabled_ || can_check_db_);

  // This object is used exclusively on the IO thread but may be constructed on
  // the UI thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SafeBrowsingUrlCheckerImpl::SafeBrowsingUrlCheckerImpl(
    network::mojom::RequestDestination request_destination,
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
    base::WeakPtr<web::WebState> weak_web_state,
    bool real_time_lookup_enabled,
    bool can_rt_check_subresource_url,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui)
    : load_flags_(0),
      request_destination_(request_destination),
      has_user_gesture_(false),
      weak_web_state_(weak_web_state),
      url_checker_delegate_(url_checker_delegate),
      database_manager_(url_checker_delegate_->GetDatabaseManager()),
      real_time_lookup_enabled_(real_time_lookup_enabled),
      can_rt_check_subresource_url_(can_rt_check_subresource_url),
      can_check_db_(true),
      ui_task_runner_(ui_task_runner),
      url_lookup_service_on_ui_(url_lookup_service_on_ui) {
  DCHECK(!can_rt_check_subresource_url_ || real_time_lookup_enabled_);

  // This object is used exclusively on the IO thread but may be constructed on
  // the UI thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SafeBrowsingUrlCheckerImpl::~SafeBrowsingUrlCheckerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == STATE_CHECKING_URL) {
    CancelCheckIfRelevant();
    const GURL& url = urls_[next_index_].url;
    TRACE_EVENT_NESTABLE_ASYNC_END1("safe_browsing", "CheckUrl",
                                    TRACE_ID_LOCAL(this), "url", url.spec());
  }
}

void SafeBrowsingUrlCheckerImpl::CheckUrl(const GURL& url,
                                          const std::string& method,
                                          CheckUrlCallback callback) {
  CheckUrlImpl(url, method, Notifier(std::move(callback)));
}

void SafeBrowsingUrlCheckerImpl::CheckUrl(const GURL& url,
                                          const std::string& method,
                                          NativeCheckUrlCallback callback) {
  CheckUrlImpl(url, method, Notifier(std::move(callback)));
}

UnsafeResource SafeBrowsingUrlCheckerImpl::MakeUnsafeResource(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    bool is_from_real_time_check,
    std::unique_ptr<RTLookupResponse> rt_lookup_response) {
  UnsafeResource resource;
  resource.url = url;
  resource.original_url = urls_[0].url;
  if (urls_.size() > 1) {
    resource.redirect_urls.reserve(urls_.size() - 1);
    for (size_t i = 1; i < urls_.size(); ++i)
      resource.redirect_urls.push_back(urls_[i].url);
  }
  resource.is_subresource =
      request_destination_ != network::mojom::RequestDestination::kDocument;
  resource.is_subframe =
      network::IsRequestDestinationEmbeddedFrame(request_destination_);
  resource.threat_type = threat_type;
  resource.threat_metadata = metadata;
  resource.request_destination = request_destination_;
  resource.callback =
      base::BindRepeating(&SafeBrowsingUrlCheckerImpl::OnBlockingPageComplete,
                          weak_factory_.GetWeakPtr());
  resource.callback_sequence = base::SequencedTaskRunner::GetCurrentDefault();
  resource.render_process_id = render_process_id_;
  resource.render_frame_id = render_frame_id_;
  resource.frame_tree_node_id = frame_tree_node_id_;
  resource.weak_web_state = weak_web_state_;
  resource.threat_source = is_from_real_time_check
                               ? ThreatSource::REAL_TIME_CHECK
                               : database_manager_->GetThreatSource();
  if (rt_lookup_response) {
    resource.rt_lookup_response = *rt_lookup_response;
  }
  return resource;
}

void SafeBrowsingUrlCheckerImpl::OnCheckBrowseUrlResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  is_async_database_manager_check_in_progress_ = false;
  OnUrlResult(url, threat_type, metadata, /*is_from_real_time_check=*/false,
              /*rt_lookup_response=*/nullptr);
}

void SafeBrowsingUrlCheckerImpl::OnUrlResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    bool is_from_real_time_check,
    std::unique_ptr<RTLookupResponse> rt_lookup_response,
    bool timed_out) {
  DCHECK_EQ(STATE_CHECKING_URL, state_);
  DCHECK_LT(next_index_, urls_.size());
  DCHECK_EQ(urls_[next_index_].url, url);

  timer_->Stop();
  RecordCheckUrlTimeout(timed_out);
  if (urls_[next_index_].is_cached_safe_url) {
    UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.GetCache.FallbackThreatType",
                              threat_type, SB_THREAT_TYPE_MAX + 1);
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1("safe_browsing", "CheckUrl",
                                  TRACE_ID_LOCAL(this), "url", url.spec());

  const bool is_prefetch = (load_flags_ & net::LOAD_PREFETCH);

  // Handle main frame and subresources. We do this to catch resources flagged
  // as phishing even if the top frame isn't flagged.
  if (!is_prefetch && threat_type == SB_THREAT_TYPE_URL_PHISHING &&
      base::FeatureList::IsEnabled(kDelayedWarnings)) {
    if (state_ != STATE_DELAYED_BLOCKING_PAGE) {
      // Delayed warnings experiment delays the warning until a user interaction
      // happens. Create an interaction observer and continue like there wasn't
      // a warning. The observer will create the interstitial when necessary.
      UnsafeResource unsafe_resource = MakeUnsafeResource(
          url, threat_type, metadata, is_from_real_time_check,
          std::move(rt_lookup_response));
      unsafe_resource.is_delayed_warning = true;
      url_checker_delegate_
          ->StartObservingInteractionsForDelayedBlockingPageHelper(
              unsafe_resource,
              request_destination_ ==
                  network::mojom::RequestDestination::kDocument);
      state_ = STATE_DELAYED_BLOCKING_PAGE;
    }
    // Let the navigation continue in case of delayed warnings.
    // No need to call ProcessUrls here, it'll return early.
    RunNextCallback(true, false);
    return;
  }

  if (threat_type == SB_THREAT_TYPE_SAFE ||
      threat_type == SB_THREAT_TYPE_SUSPICIOUS_SITE) {
    state_ = STATE_NONE;

    if (threat_type == SB_THREAT_TYPE_SUSPICIOUS_SITE) {
      url_checker_delegate_->NotifySuspiciousSiteDetected(web_contents_getter_);
    }

    if (!RunNextCallback(true, false))
      return;

    ProcessUrls();
    return;
  }

  if (is_prefetch) {
    // Destroy the prefetch with FINAL_STATUS_SAFE_BROWSING.
    if (request_destination_ == network::mojom::RequestDestination::kDocument) {
      url_checker_delegate_->MaybeDestroyNoStatePrefetchContents(
          web_contents_getter_);
    }
    // Record the result of canceled unsafe prefetch. This is used as a signal
    // for testing.
    LOCAL_HISTOGRAM_ENUMERATION(
        "SB2Test.RequestDestination.UnsafePrefetchCanceled",
        request_destination_);

    BlockAndProcessUrls(false);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("SB2.RequestDestination.Unsafe",
                            request_destination_);

  UnsafeResource resource =
      MakeUnsafeResource(url, threat_type, metadata, is_from_real_time_check,
                         std::move(rt_lookup_response));

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;
  url_checker_delegate_->StartDisplayingBlockingPageHelper(
      resource, urls_[next_index_].method, headers_,
      request_destination_ == network::mojom::RequestDestination::kDocument,
      has_user_gesture_);
}

void SafeBrowsingUrlCheckerImpl::OnTimeout() {
  CancelCheckIfRelevant();

  // Any pending callbacks on this URL check should be skipped.
  weak_factory_.InvalidateWeakPtrs();

  OnUrlResult(urls_[next_index_].url, safe_browsing::SB_THREAT_TYPE_SAFE,
              ThreatMetadata(), /*is_from_real_time_check=*/false,
              /*rt_lookup_response=*/nullptr,
              /*timed_out=*/true);
}

void SafeBrowsingUrlCheckerImpl::CheckUrlImpl(const GURL& url,
                                              const std::string& method,
                                              Notifier notifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "SafeBrowsingUrlCheckerImpl checks URL: " << url;
  urls_.emplace_back(url, method, std::move(notifier),
                     /*safe_from_real_time_cache=*/false,
                     /*did_perform_real_time_check=*/false,
                     /*did_check_allowlist=*/false);

  ProcessUrls();
}

void SafeBrowsingUrlCheckerImpl::ProcessUrls() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(STATE_BLOCKED, state_);
  if (!base::FeatureList::IsEnabled(kDelayedWarnings)) {
    DCHECK_NE(STATE_DELAYED_BLOCKING_PAGE, state_);
  }

  if (state_ == STATE_CHECKING_URL ||
      state_ == STATE_DISPLAYING_BLOCKING_PAGE ||
      state_ == STATE_DELAYED_BLOCKING_PAGE) {
    return;
  }

  while (next_index_ < urls_.size()) {
    DCHECK_EQ(STATE_NONE, state_);

    const GURL& url = urls_[next_index_].url;
    if (url_checker_delegate_->IsUrlAllowlisted(url)) {
      if (!RunNextCallback(true, false))
        return;

      continue;
    }

    // TODO(yzshen): Consider moving CanCheckRequestDestination() to the
    // renderer side. That would save some IPCs. It requires a method on the
    // SafeBrowsing mojo interface to query all supported request destinations.
    if (!database_manager_->CanCheckRequestDestination(request_destination_)) {
      UMA_HISTOGRAM_ENUMERATION("SB2.RequestDestination.Skipped",
                                request_destination_);

      if (!RunNextCallback(true, false))
        return;

      continue;
    }

    UMA_HISTOGRAM_ENUMERATION("SB2.RequestDestination.Checked",
                              request_destination_);

    SBThreatType threat_type = CheckWebUIUrls(url);
    if (threat_type != safe_browsing::SB_THREAT_TYPE_SAFE) {
      state_ = STATE_CHECKING_URL;
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
          "safe_browsing", "CheckUrl", TRACE_ID_LOCAL(this), "url", url.spec());

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&SafeBrowsingUrlCheckerImpl::OnCheckBrowseUrlResult,
                         weak_factory_.GetWeakPtr(), url, threat_type,
                         ThreatMetadata()));
      break;
    }

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("safe_browsing", "CheckUrl",
                                      TRACE_ID_LOCAL(this), "url", url.spec());

    // Start a timer to abort the check if it takes too long.
    timer_->Start(FROM_HERE, base::Milliseconds(kCheckUrlTimeoutMs), this,
                  &SafeBrowsingUrlCheckerImpl::OnTimeout);

    bool safe_synchronously;
    bool can_perform_full_url_lookup = CanPerformFullURLLookup(url);
    base::UmaHistogramBoolean("SafeBrowsing.RT.CanCheckDatabase",
                              can_check_db_);
    if (can_perform_full_url_lookup) {
      DCHECK_NE(url_lookup_service_metric_suffix_, kNoRealTimeURLLookupService);
      UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.RT.RequestDestinations.Checked",
                                request_destination_);
      urls_[next_index_].did_perform_real_time_check = true;
      safe_synchronously = false;

      bool check_allowlist =
          can_check_db_ && can_check_high_confidence_allowlist_;
      bool has_allowlist_match =
          check_allowlist &&
          database_manager_->CheckUrlForHighConfidenceAllowlist(url);
      urls_[next_index_].did_check_allowlist = check_allowlist;
      RecordLocalMatchResult(has_allowlist_match, request_destination_,
                             url_lookup_service_metric_suffix_);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &SafeBrowsingUrlCheckerImpl::OnCheckUrlForHighConfidenceAllowlist,
              weak_factory_.GetWeakPtr(),
              /*did_match_allowlist=*/has_allowlist_match));
    } else {
      safe_synchronously = can_check_db_ ? CallCheckBrowseUrl(url) : true;
    }

    if (safe_synchronously) {
      timer_->Stop();
      RecordCheckUrlTimeout(/*timed_out=*/false);

      TRACE_EVENT_NESTABLE_ASYNC_END1("safe_browsing", "CheckUrl",
                                      TRACE_ID_LOCAL(this), "url", url.spec());

      if (!RunNextCallback(true, false))
        return;

      continue;
    }

    state_ = STATE_CHECKING_URL;

    // Only send out notification of starting a slow check if the database
    // manager actually supports fast checks (i.e., synchronous checks) but is
    // not able to complete the check synchronously in this case and we're doing
    // hash-based checks.
    // Don't send out notification if the database manager doesn't support
    // synchronous checks at all (e.g., on mobile), or if performing a full URL
    // check since we don't want to block resource fetch while we perform a full
    // URL lookup. Note that we won't parse the response until the Safe Browsing
    // check is complete and return SAFE, so there's no Safe Browsing bypass
    // risk here.
    if (!can_perform_full_url_lookup &&
        !database_manager_->ChecksAreAlwaysAsync())
      urls_[next_index_].notifier.OnStartSlowCheck();

    break;
  }
}

void SafeBrowsingUrlCheckerImpl::BlockAndProcessUrls(bool showed_interstitial) {
  DVLOG(1) << "SafeBrowsingUrlCheckerImpl blocks URL: "
           << urls_[next_index_].url;
  state_ = STATE_BLOCKED;

  // If user decided to not proceed through a warning, mark all the remaining
  // redirects as "bad".
  while (next_index_ < urls_.size()) {
    if (!RunNextCallback(false, showed_interstitial))
      return;
  }
}

void SafeBrowsingUrlCheckerImpl::OnBlockingPageComplete(
    bool proceed,
    bool showed_interstitial) {
  DCHECK(state_ == STATE_DISPLAYING_BLOCKING_PAGE ||
         state_ == STATE_DELAYED_BLOCKING_PAGE);

  if (proceed) {
    state_ = STATE_NONE;
    if (!RunNextCallback(true, showed_interstitial))
      return;
    ProcessUrls();
  } else {
    BlockAndProcessUrls(showed_interstitial);
  }
}

SBThreatType SafeBrowsingUrlCheckerImpl::CheckWebUIUrls(const GURL& url) {
  if (url == kChromeUISafeBrowsingMatchMalwareUrl)
    return safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  if (url == kChromeUISafeBrowsingMatchPhishingUrl)
    return safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  if (url == kChromeUISafeBrowsingMatchUnwantedUrl)
    return safe_browsing::SB_THREAT_TYPE_URL_UNWANTED;
  if (url == kChromeUISafeBrowsingMatchBillingUrl)
    return safe_browsing::SB_THREAT_TYPE_BILLING;
  return safe_browsing::SB_THREAT_TYPE_SAFE;
}

bool SafeBrowsingUrlCheckerImpl::RunNextCallback(bool proceed,
                                                 bool showed_interstitial) {
  DCHECK_LT(next_index_, urls_.size());
  // OnCompleteCheck may delete *this*. Do not access internal members after
  // the call.
  auto weak_self = weak_factory_.GetWeakPtr();
  UrlInfo& url_info = urls_[next_index_++];
  url_info.notifier.OnCompleteCheck(proceed, showed_interstitial,
                                    url_info.did_perform_real_time_check,
                                    url_info.did_check_allowlist);

  // Careful; `this` may be destroyed.
  return !!weak_self;
}

void SafeBrowsingUrlCheckerImpl::OnCheckUrlForHighConfidenceAllowlist(
    bool did_match_allowlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_expected_request_destination =
      (network::mojom::RequestDestination::kDocument == request_destination_) ||
      (network::IsRequestDestinationEmbeddedFrame(request_destination_) &&
       can_rt_check_subresource_url_);
  DCHECK(is_expected_request_destination);

  const GURL& url = urls_[next_index_].url;
  if (did_match_allowlist) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SafeBrowsingUrlCheckerImpl::MaybeSendSampleRequest,
                       weak_factory_.GetWeakPtr(), url, last_committed_url_,
                       /*is_mainframe=*/request_destination_ ==
                           network::mojom::RequestDestination::kDocument,
                       url_lookup_service_on_ui_,
                       base::SequencedTaskRunner::GetCurrentDefault()));
    // If the URL matches the high-confidence allowlist, still do the hash based
    // checks.
    PerformHashBasedCheck(url);
    return;
  }

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SafeBrowsingUrlCheckerImpl::StartLookupOnUIThread,
                     weak_factory_.GetWeakPtr(), url, last_committed_url_,
                     /*is_mainframe=*/request_destination_ ==
                         network::mojom::RequestDestination::kDocument,
                     url_lookup_service_on_ui_,
                     base::SequencedTaskRunner::GetCurrentDefault()));
}

void SafeBrowsingUrlCheckerImpl::SetWebUIToken(int token) {
  url_web_ui_token_ = token;
}

void SafeBrowsingUrlCheckerImpl::MaybeSendSampleRequest(
    base::WeakPtr<SafeBrowsingUrlCheckerImpl> weak_checker_on_io,
    const GURL& url,
    const GURL& last_committed_url,
    bool is_mainframe,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  bool can_send_protego_sampled_ping =
      url_lookup_service_on_ui &&
      url_lookup_service_on_ui->CanSendRTSampleRequest();

  if (!can_send_protego_sampled_ping) {
    return;
  }
  bool is_lookup_service_available =
      !url_lookup_service_on_ui->IsInBackoffMode();
  if (is_lookup_service_available) {
    RTLookupRequestCallback request_callback = base::BindOnce(
        &SafeBrowsingUrlCheckerImpl::OnRTLookupRequest, weak_checker_on_io);
    url_lookup_service_on_ui->SendSampledRequest(
        url, last_committed_url, is_mainframe, std::move(request_callback),
        std::move(io_task_runner));
  }
}

// static
void SafeBrowsingUrlCheckerImpl::StartLookupOnUIThread(
    base::WeakPtr<SafeBrowsingUrlCheckerImpl> weak_checker_on_io,
    const GURL& url,
    const GURL& last_committed_url,
    bool is_mainframe,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  bool is_lookup_service_available =
      url_lookup_service_on_ui && !url_lookup_service_on_ui->IsInBackoffMode();
  base::UmaHistogramBoolean("SafeBrowsing.RT.IsLookupServiceAvailable",
                            is_lookup_service_available);
  if (!is_lookup_service_available) {
    io_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&SafeBrowsingUrlCheckerImpl::PerformHashBasedCheck,
                       weak_checker_on_io, url));
    return;
  }

  RTLookupRequestCallback request_callback = base::BindOnce(
      &SafeBrowsingUrlCheckerImpl::OnRTLookupRequest, weak_checker_on_io);

  RTLookupResponseCallback response_callback = base::BindOnce(
      &SafeBrowsingUrlCheckerImpl::OnRTLookupResponse, weak_checker_on_io);

  url_lookup_service_on_ui->StartLookup(
      url, last_committed_url, is_mainframe, std::move(request_callback),
      std::move(response_callback), std::move(io_task_runner));
}

void SafeBrowsingUrlCheckerImpl::PerformHashBasedCheck(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!can_check_db_ || CallCheckBrowseUrl(url)) {
    // No match found in the local database. Safe to call |OnUrlResult| here
    // directly.
    OnUrlResult(url, SB_THREAT_TYPE_SAFE, ThreatMetadata(),
                /*is_from_real_time_check=*/false,
                /*rt_lookup_response=*/nullptr);
  }
}

bool SafeBrowsingUrlCheckerImpl::CanPerformFullURLLookup(const GURL& url) {
  return real_time_lookup_enabled_ &&
         RealTimePolicyEngine::CanPerformFullURLLookupForRequestDestination(
             request_destination_, can_rt_check_subresource_url_) &&
         RealTimeUrlLookupServiceBase::CanCheckUrl(url);
}

void SafeBrowsingUrlCheckerImpl::OnRTLookupRequest(
    std::unique_ptr<RTLookupRequest> request,
    std::string oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LogRTLookupRequest(*request, oauth_token);
}

void SafeBrowsingUrlCheckerImpl::OnRTLookupResponse(
    bool is_rt_lookup_successful,
    bool is_cached_response,
    std::unique_ptr<RTLookupResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_expected_request_destination =
      (network::mojom::RequestDestination::kDocument == request_destination_) ||
      (network::IsRequestDestinationEmbeddedFrame(request_destination_) &&
       can_rt_check_subresource_url_);
  DCHECK(is_expected_request_destination);

  const GURL& url = urls_[next_index_].url;

  if (!is_rt_lookup_successful) {
    PerformHashBasedCheck(url);
    return;
  }

  LogRTLookupResponse(*response);

  // Filter the response to remove enterprise verdicts if experiment is not
  // enabled for Managed Policy UrlFiltering
  if (!base::FeatureList::IsEnabled((kRealTimeUrlFilteringForEnterprise))) {
    auto* response_threat_info = response->mutable_threat_info();
    auto unsupported = std::remove_if(
        response_threat_info->begin(), response_threat_info->end(),
        [](const auto& threat_info) {
          return threat_info.threat_type() ==
                 RTLookupResponse::ThreatInfo::MANAGED_POLICY;
        });
    response_threat_info->erase(unsupported, response_threat_info->end());
  }

  SBThreatType sb_threat_type = SB_THREAT_TYPE_SAFE;
  if (response && (response->threat_info_size() > 0)) {
    sb_threat_type =
        RealTimeUrlLookupServiceBase::GetSBThreatTypeForRTThreatType(
            response->threat_info(0).threat_type(),
            response->threat_info(0).verdict_type());
  }

  if (is_cached_response && sb_threat_type == SB_THREAT_TYPE_SAFE) {
    urls_[next_index_].is_cached_safe_url = true;
    PerformHashBasedCheck(url);
  } else {
    OnUrlResult(url, sb_threat_type, ThreatMetadata(),
                /*is_from_real_time_check=*/true, std::move(response),
                /*timed_out=*/false);
  }
}

void SafeBrowsingUrlCheckerImpl::LogRTLookupRequest(
    const RTLookupRequest& request,
    const std::string& oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!webui_delegate_)
    return;

  // The following is to log this RTLookupRequest on any open
  // chrome://safe-browsing pages.
  ui_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WebUIDelegate::AddToRTLookupPings,
                     base::Unretained(webui_delegate_), request, oauth_token),
      base::BindOnce(&SafeBrowsingUrlCheckerImpl::SetWebUIToken,
                     weak_factory_.GetWeakPtr()));
}

void SafeBrowsingUrlCheckerImpl::LogRTLookupResponse(
    const RTLookupResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!webui_delegate_)
    return;

  if (url_web_ui_token_ != -1) {
    // The following is to log this RTLookupResponse on any open
    // chrome://safe-browsing pages.
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebUIDelegate::AddToRTLookupResponses,
                                  base::Unretained(webui_delegate_),
                                  url_web_ui_token_, response));
  }
}

bool SafeBrowsingUrlCheckerImpl::CallCheckBrowseUrl(const GURL& url) {
  bool is_safe_synchronously = database_manager_->CheckBrowseUrl(
      url, url_checker_delegate_->GetThreatTypes(), this);
  if (!is_safe_synchronously) {
    is_async_database_manager_check_in_progress_ = true;
  }
  return is_safe_synchronously;
}

void SafeBrowsingUrlCheckerImpl::CancelCheckIfRelevant() {
  if (is_async_database_manager_check_in_progress_) {
    database_manager_->CancelCheck(this);
  }
}

void SafeBrowsingUrlCheckerImpl::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  timer_ = std::move(timer);
}

}  // namespace safe_browsing
