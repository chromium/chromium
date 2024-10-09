// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/core/browser/database_manager_mechanism.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/hash_realtime_mechanism.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/scheme_logger.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/request_destination.h"

using security_interstitials::UnsafeResource;

namespace safe_browsing {
using CompleteCheckResult = SafeBrowsingLookupMechanism::CompleteCheckResult;
using hash_realtime_utils::HashRealTimeSelection;

namespace {

// Enum used to log the action of URL checks.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CheckUrlAction {
  kChecked = 0,
  kUnsafe = 1,
  kMaxValue = kUnsafe,
};

void RecordCheckUrlTimeout(bool timed_out) {
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.CheckUrl.Timeout", timed_out);
}

void RecordCheckUrlAction(CheckUrlAction action) {
  base::UmaHistogramEnumeration("SafeBrowsing.CheckUrl.Action", action);
}

std::string GetPerformedCheckSuffix(
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  switch (performed_check) {
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck:
      return "HashDatabase";
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck:
      return "UrlRealTime";
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck:
      return "HashRealTime";
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kUnknown:
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped:
      NOTREACHED();
  }
}

}  // namespace

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(CheckUrlCallback callback)
    : callback_(std::move(callback)) {}

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(
    NativeCheckUrlCallback native_callback)
    : native_callback_(std::move(native_callback)) {}

SafeBrowsingUrlCheckerImpl::Notifier::~Notifier() = default;

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(Notifier&& other) = default;

SafeBrowsingUrlCheckerImpl::Notifier&
SafeBrowsingUrlCheckerImpl::Notifier::operator=(Notifier&& other) = default;

void SafeBrowsingUrlCheckerImpl::Notifier::OnCompleteCheck(
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped,
    PerformedCheck performed_check) {
  DCHECK(performed_check != PerformedCheck::kUnknown);
  if (callback_) {
    std::move(callback_).Run(proceed, showed_interstitial);
    return;
  }

  if (native_callback_) {
    std::move(native_callback_)
        .Run(proceed, showed_interstitial, has_post_commit_interstitial_skipped,
             performed_check);
    return;
  }
}

SafeBrowsingUrlCheckerImpl::UrlInfo::UrlInfo(const GURL& in_url,
                                             const std::string& in_method,
                                             Notifier in_notifier)
    : url(in_url), method(in_method), notifier(std::move(in_notifier)) {}

SafeBrowsingUrlCheckerImpl::UrlInfo::UrlInfo(UrlInfo&& other) = default;

SafeBrowsingUrlCheckerImpl::UrlInfo::~UrlInfo() = default;

SafeBrowsingUrlCheckerImpl::KickOffLookupMechanismResult::
    KickOffLookupMechanismResult(
        SafeBrowsingLookupMechanism::StartCheckResult start_check_result,
        PerformedCheck performed_check)
    : start_check_result(start_check_result),
      performed_check(performed_check) {}
SafeBrowsingUrlCheckerImpl::KickOffLookupMechanismResult::
    ~KickOffLookupMechanismResult() = default;

SafeBrowsingUrlCheckerImpl::SafeBrowsingUrlCheckerImpl(
    const net::HttpRequestHeaders& headers,
    int load_flags,
    bool has_user_gesture,
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    base::WeakPtr<web::WebState> weak_web_state,
    UnsafeResource::RenderProcessId render_process_id,
    const UnsafeResource::RenderFrameToken& render_frame_token,
    UnsafeResource::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id,
    bool url_real_time_lookup_enabled,
    bool can_check_db,
    bool can_check_high_confidence_allowlist,
    std::string url_lookup_service_metric_suffix,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    base::WeakPtr<HashRealTimeService> hash_realtime_service_on_ui,
    HashRealTimeSelection hash_realtime_selection,
    bool is_async_check,
    bool check_allowlist_before_hash_database,
    SessionID tab_id)
    : headers_(headers),
      load_flags_(load_flags),
      has_user_gesture_(has_user_gesture),
      web_contents_getter_(web_contents_getter),
      render_process_id_(render_process_id),
      render_frame_token_(render_frame_token),
      frame_tree_node_id_(frame_tree_node_id),
      navigation_id_(navigation_id),
      weak_web_state_(weak_web_state),
      url_checker_delegate_(std::move(url_checker_delegate)),
      database_manager_(url_checker_delegate_->GetDatabaseManager()),
      url_real_time_lookup_enabled_(url_real_time_lookup_enabled),
      can_check_db_(can_check_db),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      ui_task_runner_(ui_task_runner),
      url_lookup_service_on_ui_(url_lookup_service_on_ui),
      hash_realtime_service_on_ui_(hash_realtime_service_on_ui),
      hash_realtime_selection_(hash_realtime_selection),
      is_async_check_(is_async_check),
      check_allowlist_before_hash_database_(
          check_allowlist_before_hash_database),
      tab_id_(tab_id) {
  DCHECK(url_real_time_lookup_enabled_ || can_check_db_);
}

SafeBrowsingUrlCheckerImpl::~SafeBrowsingUrlCheckerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == STATE_CHECKING_URL) {
    const GURL& url = urls_[next_index_].url;
    TRACE_EVENT_NESTABLE_ASYNC_END1("safe_browsing", "CheckUrl",
                                    TRACE_ID_LOCAL(this), "url", url.spec());
  }
}

void SafeBrowsingUrlCheckerImpl::CheckUrl(const GURL& url,
                                          const std::string& method,
                                          CheckUrlCallback callback) {
  CheckUrlImplAndMaybeDeleteSelf(url, method, Notifier(std::move(callback)));
}

void SafeBrowsingUrlCheckerImpl::CheckUrl(const GURL& url,
                                          const std::string& method,
                                          NativeCheckUrlCallback callback) {
  CheckUrlImplAndMaybeDeleteSelf(url, method, Notifier(std::move(callback)));
}

base::WeakPtr<SafeBrowsingUrlCheckerImpl>
SafeBrowsingUrlCheckerImpl::WeakPtr() {
  return weak_factory_.GetWeakPtr();
}

UnsafeResource SafeBrowsingUrlCheckerImpl::MakeUnsafeResource(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    ThreatSource threat_source,
    std::unique_ptr<RTLookupResponse> rt_lookup_response,
    PerformedCheck performed_check) {
  UnsafeResource resource;
  resource.url = url;
  resource.original_url = urls_[0].url;
  if (urls_.size() > 1) {
    resource.redirect_urls.reserve(urls_.size() - 1);
    for (size_t i = 1; i < urls_.size(); ++i) {
      resource.redirect_urls.push_back(urls_[i].url);
    }
  }
  resource.threat_type = threat_type;
  resource.threat_metadata = metadata;
  resource.callback = base::BindRepeating(
      &SafeBrowsingUrlCheckerImpl::OnBlockingPageCompleteAndMaybeDeleteSelf,
      weak_factory_.GetWeakPtr(), performed_check);
  resource.callback_sequence = base::SequencedTaskRunner::GetCurrentDefault();
  resource.render_process_id = render_process_id_;
  resource.render_frame_token = render_frame_token_;
  resource.frame_tree_node_id = frame_tree_node_id_;
  resource.navigation_id = navigation_id_;
  resource.weak_web_state = weak_web_state_;
  resource.threat_source = threat_source;
  resource.is_async_check = is_async_check_;
  if (rt_lookup_response) {
    resource.rt_lookup_response = *rt_lookup_response;
  }
  return resource;
}

void SafeBrowsingUrlCheckerImpl::OnUrlResultAndMaybeDeleteSelf(
    PerformedCheck performed_check,
    bool timed_out,
    std::optional<std::unique_ptr<CompleteCheckResult>> result) {
  DCHECK_EQ(result.has_value(), !timed_out);
  lookup_mechanism_runner_.reset();
  if (timed_out) {
    // Any pending callbacks on this URL check should be skipped.
    weak_factory_.InvalidateWeakPtrs();
    OnUrlResultInternalAndMaybeDeleteSelf(
        urls_[next_index_].url,
        safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE, ThreatMetadata(),
        /*threat_source=*/std::nullopt,
        /*rt_lookup_response=*/nullptr,
        /*timed_out=*/true, performed_check);
  } else {
    OnUrlResultInternalAndMaybeDeleteSelf(
        result.value()->url, result.value()->threat_type,
        result.value()->metadata, result.value()->threat_source,
        std::move(result.value()->url_real_time_lookup_response),
        /*timed_out=*/false, performed_check);
  }
}

void SafeBrowsingUrlCheckerImpl::OnUrlResultInternalAndMaybeDeleteSelf(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata,
    std::optional<ThreatSource> threat_source,
    std::unique_ptr<RTLookupResponse> rt_lookup_response,
    bool timed_out,
    PerformedCheck performed_check) {
  using enum SBThreatType;

  DCHECK_EQ(STATE_CHECKING_URL, state_);
  DCHECK_LT(next_index_, urls_.size());
  DCHECK_EQ(urls_[next_index_].url, url);
  DCHECK(threat_source.has_value() || threat_type == SB_THREAT_TYPE_SAFE);

  RecordCheckUrlTimeout(timed_out);
  TRACE_EVENT_NESTABLE_ASYNC_END1("safe_browsing", "CheckUrl",
                                  TRACE_ID_LOCAL(this), "url", url.spec());

  const bool is_prefetch = (load_flags_ & net::LOAD_PREFETCH);
  base::UmaHistogramBoolean("SafeBrowsing.CheckUrl.IsDocumentCheckPrefetch",
                            is_prefetch);

  // Handle main frame and subresources. We do this to catch resources flagged
  // as phishing even if the top frame isn't flagged.
  if (!is_prefetch && threat_type == SB_THREAT_TYPE_URL_PHISHING &&
      base::FeatureList::IsEnabled(kDelayedWarnings)) {
    if (state_ != STATE_DELAYED_BLOCKING_PAGE) {
      // Delayed warnings experiment delays the warning until a user interaction
      // happens. Create an interaction observer and continue like there wasn't
      // a warning. The observer will create the interstitial when necessary.
      UnsafeResource unsafe_resource =
          MakeUnsafeResource(url, threat_type, metadata, threat_source.value(),
                             std::move(rt_lookup_response), performed_check);
      unsafe_resource.is_delayed_warning = true;
      url_checker_delegate_
          ->StartObservingInteractionsForDelayedBlockingPageHelper(
              unsafe_resource);
      state_ = STATE_DELAYED_BLOCKING_PAGE;
    }
    // Let the navigation continue in case of delayed warnings.
    // No need to call ProcessUrls here, it'll return early.
    RunNextCallbackAndMaybeDeleteSelf(
        /*proceed=*/true,
        /*showed_interstitial=*/false,
        /*has_post_commit_interstitial_skipped=*/false, performed_check);
    return;
  }

  if (threat_type == SB_THREAT_TYPE_SAFE ||
      threat_type == SB_THREAT_TYPE_SUSPICIOUS_SITE) {
    state_ = STATE_NONE;

    if (threat_type == SB_THREAT_TYPE_SUSPICIOUS_SITE) {
      url_checker_delegate_->NotifySuspiciousSiteDetected(web_contents_getter_);
    }

    if (!RunNextCallbackAndMaybeDeleteSelf(
            /*proceed=*/true,
            /*showed_interstitial=*/false,
            /*has_post_commit_interstitial_skipped=*/false, performed_check)) {
      return;
    }

    ProcessUrlsAndMaybeDeleteSelf();
    return;
  }

  if (is_prefetch) {
    // Destroy the prefetch with FINAL_STATUS_SAFE_BROWSING.
    url_checker_delegate_->MaybeDestroyNoStatePrefetchContents(
        web_contents_getter_);

    BlockAndProcessUrlsAndMaybeDeleteSelf(
        /*showed_interstitial=*/false,
        /*has_post_commit_interstitial_skipped=*/false, performed_check);
    return;
  }

  RecordCheckUrlAction(CheckUrlAction::kUnsafe);

  UnsafeResource resource =
      MakeUnsafeResource(url, threat_type, metadata, threat_source.value(),
                         std::move(rt_lookup_response), performed_check);

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;

  url_checker_delegate_->StartDisplayingBlockingPageHelper(
      resource, urls_[next_index_].method, headers_, has_user_gesture_);
}

void SafeBrowsingUrlCheckerImpl::CheckUrlImplAndMaybeDeleteSelf(
    const GURL& url,
    const std::string& method,
    Notifier notifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "SafeBrowsingUrlCheckerImpl checks URL: " << url;
  urls_.emplace_back(url, method, std::move(notifier));

  ProcessUrlsAndMaybeDeleteSelf();
}

void SafeBrowsingUrlCheckerImpl::ProcessUrlsAndMaybeDeleteSelf() {
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
      if (!RunNextCallbackAndMaybeDeleteSelf(
              /*proceed=*/true, /*showed_interstitial=*/false,
              /*has_post_commit_interstitial_skipped=*/false,
              PerformedCheck::kCheckSkipped)) {
        return;
      }

      continue;
    }

    RecordCheckUrlAction(CheckUrlAction::kChecked);

    SBThreatType threat_type = CheckWebUIUrls(url);
    if (threat_type != SBThreatType::SB_THREAT_TYPE_SAFE) {
      state_ = STATE_CHECKING_URL;
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
          "safe_browsing", "CheckUrl", TRACE_ID_LOCAL(this), "url", url.spec());

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&SafeBrowsingUrlCheckerImpl::
                             OnUrlResultInternalAndMaybeDeleteSelf,
                         weak_factory_.GetWeakPtr(), url, threat_type,
                         ThreatMetadata(),
                         database_manager_->GetBrowseUrlThreatSource(
                             CheckBrowseUrlType::kHashDatabase),
                         /*rt_lookup_response=*/nullptr, /*timed_out=*/false,
                         PerformedCheck::kCheckSkipped));
      break;
    }

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("safe_browsing", "CheckUrl",
                                      TRACE_ID_LOCAL(this), "url", url.spec());
    KickOffLookupMechanismResult result = KickOffLookupMechanism(url);

    if (result.start_check_result.is_safe_synchronously) {
      lookup_mechanism_runner_.reset();
      RecordCheckUrlTimeout(/*timed_out=*/false);

      TRACE_EVENT_NESTABLE_ASYNC_END1("safe_browsing", "CheckUrl",
                                      TRACE_ID_LOCAL(this), "url", url.spec());

      if (!RunNextCallbackAndMaybeDeleteSelf(
              /*proceed=*/true,
              /*showed_interstitial=*/false,
              /*has_post_commit_interstitial_skipped=*/false,
              result.performed_check)) {
        return;
      }

      continue;
    }

    state_ = STATE_CHECKING_URL;

    break;
  }
}

std::unique_ptr<SafeBrowsingLookupMechanism>
SafeBrowsingUrlCheckerImpl::GetHashRealTimeLookupMechanism(
    const GURL& url,
    bool can_use_hash_real_time_service,
    bool can_use_hash_real_time_db_manager) {
  CHECK(can_use_hash_real_time_service || can_use_hash_real_time_db_manager);
  if (can_use_hash_real_time_service) {
    return std::make_unique<HashRealTimeMechanism>(
        url, url_checker_delegate_->GetThreatTypes(), database_manager_,
        ui_task_runner_, hash_realtime_service_on_ui_);
  }
  return std::make_unique<DatabaseManagerMechanism>(
      url, url_checker_delegate_->GetThreatTypes(), database_manager_,
      CheckBrowseUrlType::kHashRealTime,
      /*check_allowlist=*/false);
}

SafeBrowsingUrlCheckerImpl::KickOffLookupMechanismResult
SafeBrowsingUrlCheckerImpl::KickOffLookupMechanism(const GURL& url) {
  base::UmaHistogramBoolean("SafeBrowsing.RT.CanCheckDatabase", can_check_db_);
  scheme_logger::LogScheme(url, "SafeBrowsing.CheckUrl.UrlScheme");
  std::unique_ptr<SafeBrowsingLookupMechanism> lookup_mechanism;
  PerformedCheck performed_check = PerformedCheck::kUnknown;
  DCHECK(!lookup_mechanism_runner_);
  bool can_use_hash_real_time_service =
      hash_realtime_selection_ == HashRealTimeSelection::kHashRealTimeService &&
      HashRealTimeService::CanCheckUrl(url);
  bool can_use_hash_real_time_db_manager =
      hash_realtime_selection_ == HashRealTimeSelection::kDatabaseManager &&
      hash_realtime_utils::CanCheckUrl(url);
  if (CanPerformFullURLLookup(url)) {
    performed_check = PerformedCheck::kUrlRealTimeCheck;

    // For ESB users, we will sample eligible lookups and send both Protego and
    // HPRT lookups based on the configurable percentage. Otherwise, perform URL
    // real-time lookup only.
    bool should_run_background_hprt_check =
        base::FeatureList::IsEnabled(kHashPrefixRealTimeLookupsSamplePing) &&
        url_checker_delegate_->AreBackgroundHashRealTimeSampleLookupsAllowed(
            web_contents_getter_) &&
        base::RandDouble() * 100 < kHashPrefixRealTimeLookupsSampleRate.Get() &&
        (can_use_hash_real_time_service || can_use_hash_real_time_db_manager);
    lookup_mechanism = std::make_unique<UrlRealTimeMechanism>(
        url, url_checker_delegate_->GetThreatTypes(), database_manager_,
        can_check_db_, can_check_high_confidence_allowlist_,
        url_lookup_service_metric_suffix_, ui_task_runner_,
        url_lookup_service_on_ui_, url_checker_delegate_, web_contents_getter_,
        tab_id_,
        should_run_background_hprt_check
            ? GetHashRealTimeLookupMechanism(url,
                                             can_use_hash_real_time_service,
                                             can_use_hash_real_time_db_manager)
            : nullptr);
  } else if (!can_check_db_) {
    return KickOffLookupMechanismResult(
        SafeBrowsingLookupMechanism::StartCheckResult(
            /*is_safe_synchronously=*/true, /*threat_source=*/std::nullopt),
        PerformedCheck::kCheckSkipped);
  } else if (can_use_hash_real_time_service) {
    performed_check = PerformedCheck::kHashRealTimeCheck;
    lookup_mechanism = std::make_unique<HashRealTimeMechanism>(
        url, url_checker_delegate_->GetThreatTypes(), database_manager_,
        ui_task_runner_, hash_realtime_service_on_ui_);
  } else if (can_use_hash_real_time_db_manager) {
    performed_check = PerformedCheck::kHashRealTimeCheck;
    lookup_mechanism = std::make_unique<DatabaseManagerMechanism>(
        url, url_checker_delegate_->GetThreatTypes(), database_manager_,
        CheckBrowseUrlType::kHashRealTime,
        /*check_allowlist=*/false);
  } else {
    performed_check = PerformedCheck::kHashDatabaseCheck;
    lookup_mechanism = std::make_unique<DatabaseManagerMechanism>(
        url, url_checker_delegate_->GetThreatTypes(), database_manager_,
        CheckBrowseUrlType::kHashDatabase,
        /*check_allowlist=*/check_allowlist_before_hash_database_);
  }
  DCHECK(performed_check != PerformedCheck::kUnknown);
  lookup_mechanism_runner_ =
      std::make_unique<SafeBrowsingLookupMechanismRunner>(
          std::move(lookup_mechanism), GetPerformedCheckSuffix(performed_check),
          base::BindOnce(
              &SafeBrowsingUrlCheckerImpl::OnUrlResultAndMaybeDeleteSelf,
              weak_factory_.GetWeakPtr(), performed_check));
  return KickOffLookupMechanismResult(lookup_mechanism_runner_->Run(),
                                      performed_check);
}

void SafeBrowsingUrlCheckerImpl::BlockAndProcessUrlsAndMaybeDeleteSelf(
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped,
    PerformedCheck performed_check) {
  DVLOG(1) << "SafeBrowsingUrlCheckerImpl blocks URL: "
           << urls_[next_index_].url;
  state_ = STATE_BLOCKED;

  // If user decided to not proceed through a warning, mark all the remaining
  // redirects as "bad".
  while (next_index_ < urls_.size()) {
    if (!RunNextCallbackAndMaybeDeleteSelf(
            /*proceed=*/false, showed_interstitial,
            has_post_commit_interstitial_skipped, performed_check)) {
      return;
    }
  }
}

void SafeBrowsingUrlCheckerImpl::OnBlockingPageCompleteAndMaybeDeleteSelf(
    PerformedCheck performed_check,
    UnsafeResource::UrlCheckResult result) {
  DCHECK(state_ == STATE_DISPLAYING_BLOCKING_PAGE ||
         state_ == STATE_DELAYED_BLOCKING_PAGE);

  if (result.proceed) {
    state_ = STATE_NONE;
    if (!RunNextCallbackAndMaybeDeleteSelf(
            /*proceed=*/true, result.showed_interstitial,
            result.has_post_commit_interstitial_skipped, performed_check)) {
      return;
    }
    ProcessUrlsAndMaybeDeleteSelf();
  } else {
    BlockAndProcessUrlsAndMaybeDeleteSelf(
        result.showed_interstitial, result.has_post_commit_interstitial_skipped,
        performed_check);
  }
}

SBThreatType SafeBrowsingUrlCheckerImpl::CheckWebUIUrls(const GURL& url) {
  using enum SBThreatType;
  if (url == kChromeUISafeBrowsingMatchMalwareUrl) {
    return SB_THREAT_TYPE_URL_MALWARE;
  }
  if (url == kChromeUISafeBrowsingMatchPhishingUrl) {
    return SB_THREAT_TYPE_URL_PHISHING;
  }
  if (url == kChromeUISafeBrowsingMatchUnwantedUrl) {
    return SB_THREAT_TYPE_URL_UNWANTED;
  }
  if (url == kChromeUISafeBrowsingMatchBillingUrl) {
    return SB_THREAT_TYPE_BILLING;
  }
  return SB_THREAT_TYPE_SAFE;
}

bool SafeBrowsingUrlCheckerImpl::RunNextCallbackAndMaybeDeleteSelf(
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped,
    PerformedCheck performed_check) {
  DCHECK_LT(next_index_, urls_.size());
  // OnCompleteCheck may delete *this*. Do not access internal members after
  // the call.
  auto weak_self = weak_factory_.GetWeakPtr();
  UrlInfo& url_info = urls_[next_index_++];
  url_info.notifier.OnCompleteCheck(proceed, showed_interstitial,
                                    has_post_commit_interstitial_skipped,
                                    performed_check);

  // Careful; `this` may be destroyed.
  return !!weak_self;
}

bool SafeBrowsingUrlCheckerImpl::CanPerformFullURLLookup(const GURL& url) {
  return url_real_time_lookup_enabled_ && url_lookup_service_on_ui_ &&
         url_lookup_service_on_ui_->CanCheckUrl(url);
}

}  // namespace safe_browsing
