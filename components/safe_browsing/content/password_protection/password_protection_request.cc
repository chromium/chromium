// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/password_protection/password_protection_request.h"

#include <cstddef>
#include <memory>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/content/password_protection/metrics_util.h"
#include "components/safe_browsing/content/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/content/password_protection/visual_utils.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/db/allowlist_checker_client.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/url_formatter/url_formatter.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserThread;
using content::WebContents;

namespace safe_browsing {

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;

namespace {

// Cap on how many reused domains can be included in a report, to limit
// the size of the report. UMA suggests 99.9% will have < 200 domains.
const int kMaxReusedDomains = 200;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
// The maximum time to wait for DOM features to be collected, in milliseconds.
const int kDomFeatureTimeoutMs = 3000;
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
// Parameters chosen to ensure privacy is preserved by visual features.
const int kMinWidthForVisualFeatures = 576;
const int kMinHeightForVisualFeatures = 576;
const float kMaxZoomForVisualFeatures = 2.0;

std::unique_ptr<VisualFeatures> ExtractVisualFeatures(
    const SkBitmap& screenshot) {
  auto features = std::make_unique<VisualFeatures>();
  visual_utils::GetHistogramForImage(screenshot,
                                     features->mutable_color_histogram());
  visual_utils::GetBlurredImage(screenshot, features->mutable_image());
  return features;
}
#endif

std::vector<std::string> GetMatchingDomains(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  std::vector<std::string> matching_domains;
  matching_domains.reserve(matching_reused_credentials.size());
  for (const auto& credential : matching_reused_credentials) {
    // This only works for Web credentials. For Android credentials, there needs
    // to be special handing and should use affiliation information instead of
    // the signon_realm.
    std::string domain = base::UTF16ToUTF8(url_formatter::FormatUrl(
        GURL(credential.signon_realm),
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    matching_domains.push_back(std::move(domain));
  }
  return base::flat_set<std::string>(std::move(matching_domains)).extract();
}

}  // namespace

PasswordProtectionRequest::PasswordProtectionRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    const std::string& username,
    PasswordType password_type,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    LoginReputationClientRequest::TriggerType type,
    bool password_field_exists,
    PasswordProtectionService* pps,
    int request_timeout_in_ms)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      main_frame_url_(main_frame_url),
      password_form_action_(password_form_action),
      password_form_frame_url_(password_form_frame_url),
      username_(username),
      password_type_(password_type),
      matching_domains_(GetMatchingDomains(matching_reused_credentials)),
      matching_reused_credentials_(matching_reused_credentials),
      trigger_type_(type),
      password_field_exists_(password_field_exists),
      password_protection_service_(pps),
      request_timeout_in_ms_(request_timeout_in_ms),
      request_proto_(std::make_unique<LoginReputationClientRequest>()),
      is_modal_warning_showing_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(trigger_type_ == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type_ == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  DCHECK(trigger_type_ != LoginReputationClientRequest::PASSWORD_REUSE_EVENT ||
         password_type_ != PasswordType::SAVED_PASSWORD ||
         !matching_reused_credentials_.empty());
  request_proto_->set_trigger_type(trigger_type_);
  *request_proto_->mutable_url_display_experiment() =
      pps->GetUrlDisplayExperiment();
}

PasswordProtectionRequest::~PasswordProtectionRequest() {
  weakptr_factory_.InvalidateWeakPtrs();
}

void PasswordProtectionRequest::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CheckWhitelist();
}

void PasswordProtectionRequest::CheckWhitelist() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // In order to send pings for about:blank, we skip the whitelist check for
  // URLs with unsupported schemes.
  if (!password_protection_service_->database_manager()->CanCheckUrl(
          main_frame_url_)) {
    OnWhitelistCheckDone(false);
    return;
  }

  // Start a task on the IO thread to check the whitelist. It may
  // callback immediately on the IO thread or take some time if a full-hash-
  // check is required.
  auto result_callback =
      base::BindOnce(&OnWhitelistCheckDoneOnIO, GetWeakPtr());
  tracker_.PostTask(
      content::GetIOThreadTaskRunner({}).get(), FROM_HERE,
      base::BindOnce(&AllowlistCheckerClient::StartCheckCsdWhitelist,
                     password_protection_service_->database_manager(),
                     main_frame_url_, std::move(result_callback)));
}

// static
void PasswordProtectionRequest::OnWhitelistCheckDoneOnIO(
    base::WeakPtr<PasswordProtectionRequest> weak_request,
    bool match_whitelist) {
  // Don't access weak_request on IO thread. Move it back to UI thread first.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordProtectionRequest::OnWhitelistCheckDone,
                     weak_request, match_whitelist));
}

void PasswordProtectionRequest::OnWhitelistCheckDone(bool match_whitelist) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (match_whitelist) {
    if (password_protection_service_->CanSendSamplePing()) {
      FillRequestProto(/*is_sampled_ping=*/true);
    }
    Finish(RequestOutcome::MATCHED_WHITELIST, nullptr);
  } else {
    // In case the request to Safe Browsing takes too long,
    // we set a timer to cancel that request and return an "unspecified verdict"
    // so that the navigation isn't blocked indefinitely.
    StartTimeout();
    CheckCachedVerdicts();
  }
}

void PasswordProtectionRequest::CheckCachedVerdicts() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!password_protection_service_) {
    Finish(RequestOutcome::SERVICE_DESTROYED, nullptr);
    return;
  }

  std::unique_ptr<LoginReputationClientResponse> cached_response =
      std::make_unique<LoginReputationClientResponse>();
  ReusedPasswordAccountType password_account_type =
      password_protection_service_
          ->GetPasswordProtectionReusedPasswordAccountType(password_type_,
                                                           username_);

  auto verdict = password_protection_service_->GetCachedVerdict(
      main_frame_url_, trigger_type_, password_account_type,
      cached_response.get());

  if (verdict != LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED) {
    set_request_outcome(RequestOutcome::RESPONSE_ALREADY_CACHED);
    Finish(RequestOutcome::RESPONSE_ALREADY_CACHED, std::move(cached_response));
  } else {
    FillRequestProto(/*is_sampled_ping=*/false);
  }
}

void PasswordProtectionRequest::FillRequestProto(bool is_sampled_ping) {
  request_proto_->set_page_url(main_frame_url_.spec());
  LoginReputationClientRequest::Frame* main_frame =
      request_proto_->add_frames();
  main_frame->set_url(main_frame_url_.spec());
  main_frame->set_frame_index(0 /* main frame */);
  password_protection_service_->FillReferrerChain(
      main_frame_url_, SessionID::InvalidValue(), main_frame);

  // If a sample ping is send, only the URL and referrer chain is sent in the
  // request.
  if (is_sampled_ping) {
    LogPasswordProtectionSampleReportSent();
    request_proto_->set_report_type(
        LoginReputationClientRequest::SAMPLE_REPORT);
    request_proto_->clear_trigger_type();
    if (main_frame->referrer_chain_size() > 0) {
      password_protection_service_->SanitizeReferrerChain(
          main_frame->mutable_referrer_chain());
    }
    SendRequest();
    return;
  } else {
    request_proto_->set_report_type(LoginReputationClientRequest::FULL_REPORT);
  }

  password_protection_service_->FillUserPopulation(trigger_type_,
                                                   request_proto_.get());
  request_proto_->set_stored_verdict_cnt(
      password_protection_service_->GetStoredVerdictCount(trigger_type_));

  bool clicked_through_interstitial =
      password_protection_service_->UserClickedThroughSBInterstitial(
          web_contents_);
  request_proto_->set_clicked_through_interstitial(
      clicked_through_interstitial);
  request_proto_->set_content_type(web_contents_->GetContentsMimeType());

#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (password_protection_service_->IsExtendedReporting() &&
      !password_protection_service_->IsIncognito()) {
    gfx::Size content_area_size =
        password_protection_service_->GetCurrentContentAreaSize();
    request_proto_->set_content_area_height(content_area_size.height());
    request_proto_->set_content_area_width(content_area_size.width());
  }
#endif

  switch (trigger_type_) {
    case LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE: {
      LoginReputationClientRequest::Frame::Form* password_form;
      if (password_form_frame_url_ == main_frame_url_) {
        main_frame->set_has_password_field(true);
        password_form = main_frame->add_forms();
      } else {
        LoginReputationClientRequest::Frame* password_frame =
            request_proto_->add_frames();
        password_frame->set_url(password_form_frame_url_.spec());
        password_frame->set_has_password_field(true);
        password_form = password_frame->add_forms();
      }
      password_form->set_action_url(password_form_action_.spec());
      break;
    }
    case LoginReputationClientRequest::PASSWORD_REUSE_EVENT: {
      main_frame->set_has_password_field(password_field_exists_);
      LoginReputationClientRequest::PasswordReuseEvent* reuse_event =
          request_proto_->mutable_password_reuse_event();
      bool matches_signin_password =
          password_type_ == PasswordType::PRIMARY_ACCOUNT_PASSWORD;
      reuse_event->set_is_chrome_signin_password(matches_signin_password);
      reuse_event->set_reused_password_type(
          password_protection_service_->GetPasswordProtectionReusedPasswordType(
              password_type_));
      if (matches_signin_password) {
        reuse_event->set_sync_account_type(
            password_protection_service_->GetSyncAccountType());
        LogSyncAccountType(reuse_event->sync_account_type());
      }

      if (password_protection_service_->IsExtendedReporting() &&
          !password_protection_service_->IsIncognito()) {
        for (const auto& domain : matching_domains_) {
          reuse_event->add_domains_matching_password(domain);
          if (reuse_event->domains_matching_password_size() >=
              kMaxReusedDomains)
            break;
        }
      }
      if (base::FeatureList::IsEnabled(
              safe_browsing::kPasswordProtectionForSignedInUsers)) {
        ReusedPasswordAccountType password_account_type_to_add =
            password_protection_service_
                ->GetPasswordProtectionReusedPasswordAccountType(password_type_,
                                                                 username_);
        *reuse_event->mutable_reused_password_account_type() =
            password_account_type_to_add;
      }
      break;
    }
    default:
      NOTREACHED();
  }

  bool client_side_detection_enabled =
#if BUILDFLAG(FULL_SAFE_BROWSING)
      true;
#else
      base::FeatureList::IsEnabled(
          safe_browsing::kClientSideDetectionForAndroid);
#endif

  if (!client_side_detection_enabled) {
    SendRequest();
    return;
  }

  // Get the page DOM features.
  content::RenderFrameHost* rfh = web_contents_->GetMainFrame();
  password_protection_service_->GetPhishingDetector(rfh->GetRemoteInterfaces(),
                                                    &phishing_detector_);
  dom_features_collection_complete_ = false;
  phishing_detector_->StartPhishingDetection(
      main_frame_url_,
      base::BindRepeating(&PasswordProtectionRequest::OnGetDomFeatures,
                          GetWeakPtr()));
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordProtectionRequest::OnGetDomFeatureTimeout,
                     GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDomFeatureTimeoutMs));
  dom_feature_start_time_ = base::TimeTicks::Now();
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
void PasswordProtectionRequest::OnGetDomFeatures(
    mojom::PhishingDetectorResult result,
    const std::string& verdict) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (dom_features_collection_complete_)
    return;

  UMA_HISTOGRAM_ENUMERATION("PasswordProtection.RendererDomFeatureResult",
                            result);

  if (result != mojom::PhishingDetectorResult::SUCCESS &&
      result != mojom::PhishingDetectorResult::INVALID_SCORE)
    return;

  dom_features_collection_complete_ = true;
  ClientPhishingRequest dom_features_request;
  if (dom_features_request.ParseFromString(verdict)) {
    for (const ClientPhishingRequest::Feature& feature :
         dom_features_request.feature_map()) {
      DomFeatures::Feature* new_feature =
          request_proto_->mutable_dom_features()->add_feature_map();
      new_feature->set_name(feature.name());
      new_feature->set_value(feature.value());
    }

    for (const ClientPhishingRequest::Feature& feature :
         dom_features_request.non_model_feature_map()) {
      DomFeatures::Feature* new_feature =
          request_proto_->mutable_dom_features()->add_feature_map();
      new_feature->set_name(feature.name());
      new_feature->set_value(feature.value());
    }

    request_proto_->mutable_dom_features()->mutable_shingle_hashes()->Swap(
        dom_features_request.mutable_shingle_hashes());
    request_proto_->mutable_dom_features()->set_model_version(
        dom_features_request.model_version());
  }

  UMA_HISTOGRAM_TIMES("PasswordProtection.DomFeatureExtractionDuration",
                      base::TimeTicks::Now() - dom_feature_start_time_);

  MaybeCollectVisualFeatures();
}

void PasswordProtectionRequest::OnGetDomFeatureTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!dom_features_collection_complete_) {
    dom_features_collection_complete_ = true;
    MaybeCollectVisualFeatures();
  }
}

void PasswordProtectionRequest::MaybeCollectVisualFeatures() {
#if BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  SendRequest();
#else
  // Once the DOM features are collected, either collect visual features, or go
  // straight to sending the ping.
  if (trigger_type_ == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE &&
      password_protection_service_->IsExtendedReporting() &&
      zoom::ZoomController::GetZoomLevelForWebContents(web_contents_) <=
          kMaxZoomForVisualFeatures &&
      request_proto_->content_area_width() >= kMinWidthForVisualFeatures &&
      request_proto_->content_area_height() >= kMinHeightForVisualFeatures) {
    CollectVisualFeatures();
  } else {
    SendRequest();
  }
#endif  // BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
}
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(FULL_SAFE_BROWSING)
void PasswordProtectionRequest::CollectVisualFeatures() {
  content::RenderWidgetHostView* view =
      web_contents_ ? web_contents_->GetRenderWidgetHostView() : nullptr;

  if (!view) {
    SendRequest();
    return;
  }

  visual_feature_start_time_ = base::TimeTicks::Now();

  view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindOnce(&PasswordProtectionRequest::OnScreenshotTaken,
                     GetWeakPtr()));
}

void PasswordProtectionRequest::OnScreenshotTaken(const SkBitmap& screenshot) {
  // Do the feature extraction on a worker thread, to avoid blocking the UI.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ExtractVisualFeatures, screenshot),
      base::BindOnce(&PasswordProtectionRequest::OnVisualFeatureCollectionDone,
                     GetWeakPtr()));
}

void PasswordProtectionRequest::OnVisualFeatureCollectionDone(
    std::unique_ptr<VisualFeatures> visual_features) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  request_proto_->mutable_visual_features()->Swap(visual_features.get());

  UMA_HISTOGRAM_TIMES("PasswordProtection.VisualFeatureExtractionDuration",
                      base::TimeTicks::Now() - visual_feature_start_time_);

  SendRequest();
}
#endif

void PasswordProtectionRequest::SendRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui_token_ =
      WebUIInfoSingleton::GetInstance()->AddToPGPings(*request_proto_);

  std::string serialized_request;
  if (!request_proto_->SerializeToString(&serialized_request)) {
    Finish(RequestOutcome::REQUEST_MALFORMED, nullptr);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("password_protection_request", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When the user is about to log in to a new, uncommon site, Chrome "
            "will send a request to Safe Browsing to determine if the page is "
            "phishing. It'll then show a warning if the page poses a risk of "
            "phishing."
          trigger:
            "When a user focuses on a password field on a page that they "
            "haven't visited before and that isn't popular or known to be safe."
          data:
            "URL and referrer of the current page, password form action, and "
            "iframe structure."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting:
            "Users can control this feature via 'Protect you and your device "
            "from dangerous sites'. By default, this setting is enabled."
            "Alternatively, you can turn it off via "
            "'PasswordProtectionWarningTrigger' enterprise policy setting."
          chrome_policy {
            PasswordProtectionWarningTrigger {
              policy_options {mode: MANDATORY}
              PasswordProtectionWarningTrigger: 2
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      PasswordProtectionService::GetPasswordProtectionRequestUrl();
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(serialized_request,
                                     "application/octet-stream");
  request_start_time_ = base::TimeTicks::Now();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      password_protection_service_->url_loader_factory().get(),
      base::BindOnce(&PasswordProtectionRequest::OnURLLoaderComplete,
                     base::Unretained(this)));
}

void PasswordProtectionRequest::StartTimeout() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If request is not done withing 10 seconds, we cancel this request.
  // The weak pointer used for the timeout will be invalidated (and
  // hence would prevent the timeout) if the check completes on time and
  // execution reaches Finish().
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordProtectionRequest::Cancel, GetWeakPtr(), true),
      base::TimeDelta::FromMilliseconds(request_timeout_in_ms_));
}

void PasswordProtectionRequest::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  const bool is_success = url_loader_->NetError() == net::OK;

  LogPasswordProtectionNetworkResponseAndDuration(
      is_success ? response_code : url_loader_->NetError(),
      request_start_time_);

  if (!is_success || net::HTTP_OK != response_code) {
    Finish(RequestOutcome::FETCH_FAILED, nullptr);
    return;
  }

  std::unique_ptr<LoginReputationClientResponse> response =
      std::make_unique<LoginReputationClientResponse>();
  DCHECK(response_body);
  url_loader_.reset();  // We don't need it anymore.
  if (response_body && response->ParseFromString(*response_body)) {
    WebUIInfoSingleton::GetInstance()->AddToPGResponses(web_ui_token_,
                                                        *response);
    set_request_outcome(RequestOutcome::SUCCEEDED);
    Finish(RequestOutcome::SUCCEEDED, std::move(response));
  } else {
    Finish(RequestOutcome::RESPONSE_MALFORMED, nullptr);
  }
}

void PasswordProtectionRequest::Finish(
    RequestOutcome outcome,
    std::unique_ptr<LoginReputationClientResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_.TryCancelAll();

  // If the request is canceled, the PasswordProtectionService is already
  // partially destroyed, and we won't be able to log accurate metrics.
  if (outcome != RequestOutcome::CANCELED) {
    ReusedPasswordAccountType password_account_type =
        password_protection_service_
            ->GetPasswordProtectionReusedPasswordAccountType(password_type_,
                                                             username_);
    if (trigger_type_ == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
      LogPasswordOnFocusRequestOutcome(outcome);
    } else {
#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
      LogPasswordEntryRequestOutcome(outcome, password_account_type);
#endif

#if defined(PASSWORD_REUSE_WARNING_ENABLED)
      if (password_type_ == PasswordType::PRIMARY_ACCOUNT_PASSWORD) {
        password_protection_service_->MaybeLogPasswordReuseLookupEvent(
            web_contents_, outcome, password_type_, response.get());
      }
#endif
    }

    if (outcome == RequestOutcome::SUCCEEDED && response) {
      LogPasswordProtectionVerdict(trigger_type_, password_account_type,
                                   response->verdict_type());
    }
  }
  password_protection_service_->RequestFinished(this, outcome,
                                                std::move(response));
}

void PasswordProtectionRequest::Cancel(bool timed_out) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  url_loader_.reset();
  // If request is canceled because |password_protection_service_| is shutting
  // down, ignore all these deferred navigations.
  if (!timed_out) {
    throttles_.clear();
  }

  Finish(timed_out ? RequestOutcome::TIMEDOUT : RequestOutcome::CANCELED,
         nullptr);
}

void PasswordProtectionRequest::HandleDeferredNavigations() {
  for (auto* throttle : throttles_) {
    if (is_modal_warning_showing_)
      throttle->CancelNavigation(content::NavigationThrottle::CANCEL);
    else
      throttle->ResumeNavigation();
  }
  throttles_.clear();
}

void PasswordProtectionRequest::WebContentsDestroyed() {
  Cancel(/*timed_out=*/false);
}

}  // namespace safe_browsing
