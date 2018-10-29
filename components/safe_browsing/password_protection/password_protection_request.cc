// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/password_protection_request.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/db/whitelist_checker_client.h"
#include "components/safe_browsing/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

using content::BrowserThread;
using content::WebContents;

namespace safe_browsing {

namespace {

// Cap on how many reused domains can be included in a report, to limit
// the size of the report. UMA suggests 99.9% will have < 200 domains.
const int kMaxReusedDomains = 200;

}  // namespace

PasswordProtectionRequest::PasswordProtectionRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    ReusedPasswordType reused_password_type,
    const std::vector<std::string>& matching_domains,
    LoginReputationClientRequest::TriggerType type,
    bool password_field_exists,
    PasswordProtectionService* pps,
    int request_timeout_in_ms)
    : web_contents_(web_contents),
      main_frame_url_(main_frame_url),
      password_form_action_(password_form_action),
      password_form_frame_url_(password_form_frame_url),
      reused_password_type_(reused_password_type),
      matching_domains_(matching_domains),
      trigger_type_(type),
      password_field_exists_(password_field_exists),
      password_protection_service_(pps),
      request_timeout_in_ms_(request_timeout_in_ms),
      request_proto_(std::make_unique<LoginReputationClientRequest>()),
      is_modal_warning_showing_(false),
      weakptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(trigger_type_ == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type_ == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  DCHECK(trigger_type_ != LoginReputationClientRequest::PASSWORD_REUSE_EVENT ||
         reused_password_type_ !=
             LoginReputationClientRequest::PasswordReuseEvent::SAVED_PASSWORD ||
         matching_domains_.size() > 0);

  request_proto_->set_trigger_type(trigger_type_);
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
  auto result_callback = base::Bind(&OnWhitelistCheckDoneOnIO, GetWeakPtr());
  tracker_.PostTask(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}).get(),
      FROM_HERE,
      base::BindOnce(&WhitelistCheckerClient::StartCheckCsdWhitelist,
                     password_protection_service_->database_manager(),
                     main_frame_url_, result_callback));
}

// static
void PasswordProtectionRequest::OnWhitelistCheckDoneOnIO(
    base::WeakPtr<PasswordProtectionRequest> weak_request,
    bool match_whitelist) {
  // Don't access weak_request on IO thread. Move it back to UI thread first.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&PasswordProtectionRequest::OnWhitelistCheckDone,
                     weak_request, match_whitelist));
}

void PasswordProtectionRequest::OnWhitelistCheckDone(bool match_whitelist) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (match_whitelist)
    Finish(RequestOutcome::MATCHED_WHITELIST, nullptr);
  else
    CheckCachedVerdicts();
}

void PasswordProtectionRequest::CheckCachedVerdicts() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!password_protection_service_) {
    Finish(RequestOutcome::SERVICE_DESTROYED, nullptr);
    return;
  }

  std::unique_ptr<LoginReputationClientResponse> cached_response =
      std::make_unique<LoginReputationClientResponse>();
  auto verdict = password_protection_service_->GetCachedVerdict(
      main_frame_url_, trigger_type_, reused_password_type_,
      cached_response.get());
  if (verdict != LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED)
    Finish(RequestOutcome::RESPONSE_ALREADY_CACHED, std::move(cached_response));
  else
    SendRequest();
}

void PasswordProtectionRequest::FillRequestProto() {
  request_proto_->set_page_url(main_frame_url_.spec());
  password_protection_service_->FillUserPopulation(trigger_type_,
                                                   request_proto_.get());
  request_proto_->set_stored_verdict_cnt(
      password_protection_service_->GetStoredVerdictCount(trigger_type_));
  LoginReputationClientRequest::Frame* main_frame =
      request_proto_->add_frames();
  main_frame->set_url(main_frame_url_.spec());
  main_frame->set_frame_index(0 /* main frame */);
  password_protection_service_->FillReferrerChain(
      main_frame_url_, SessionID::InvalidValue(), main_frame);
  bool clicked_through_interstitial =
      password_protection_service_->UserClickedThroughSBInterstitial(
          web_contents_);
  request_proto_->set_clicked_through_interstitial(
      clicked_through_interstitial);
  request_proto_->set_content_type(web_contents_->GetContentsMimeType());
  if (password_protection_service_->IsExtendedReporting() &&
      !password_protection_service_->IsIncognito()) {
    gfx::Size content_area_size =
        password_protection_service_->GetCurrentContentAreaSize();
    request_proto_->set_content_area_height(content_area_size.height());
    request_proto_->set_content_area_width(content_area_size.width());
  }

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
      bool matches_sync_password =
          reused_password_type_ ==
          LoginReputationClientRequest::PasswordReuseEvent::SIGN_IN_PASSWORD;
      reuse_event->set_is_chrome_signin_password(matches_sync_password);
      reuse_event->set_reused_password_type(reused_password_type_);
      if (matches_sync_password) {
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
      break;
    }
    default:
      NOTREACHED();
  }
}

void PasswordProtectionRequest::SendRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FillRequestProto();

  web_ui_token_ =
      WebUIInfoSingleton::GetInstance()->AddToPGPings(*request_proto_);

  std::string serialized_request;
  if (!request_proto_->SerializeToString(&serialized_request)) {
    Finish(RequestOutcome::REQUEST_MALFORMED, nullptr);
    return;
  }

  // In case the request take too long, we set a timer to cancel this request.
  StartTimeout();
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
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
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
  if (trigger_type_ == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
    LogPasswordOnFocusRequestOutcome(outcome);
  } else {
    LogPasswordEntryRequestOutcome(
        outcome, reused_password_type_,
        password_protection_service_->GetSyncAccountType());
    if (reused_password_type_ ==
        LoginReputationClientRequest::PasswordReuseEvent::SIGN_IN_PASSWORD) {
      password_protection_service_->MaybeLogPasswordReuseLookupEvent(
          web_contents_, outcome, response.get());
    }
  }

  if (outcome == RequestOutcome::SUCCEEDED && response) {
    LogPasswordProtectionVerdict(
        trigger_type_, reused_password_type_,
        password_protection_service_->GetSyncAccountType(),
        response->verdict_type());
  }

  password_protection_service_->RequestFinished(
      this, outcome == RequestOutcome::RESPONSE_ALREADY_CACHED,
      std::move(response));
}

void PasswordProtectionRequest::Cancel(bool timed_out) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  url_loader_.reset();
  // If request is canceled because |password_protection_service_| is shutting
  // down, ignore all these deferred navigations.
  if (!timed_out)
    throttles_.clear();

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

}  // namespace safe_browsing
