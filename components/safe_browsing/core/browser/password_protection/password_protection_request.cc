// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/password_protection/password_protection_request.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/allowlist_checker_client.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/password_protection/password_protection_service_base.h"
#include "components/safe_browsing/core/browser/user_population.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace safe_browsing {

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;

namespace {

// Cap on how many reused domains can be included in a report, to limit
// the size of the report. UMA suggests 99.9% will have < 200 domains.
const int kMaxReusedDomains = 200;

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
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    matching_domains.push_back(std::move(domain));
  }
  return base::flat_set<std::string>(std::move(matching_domains)).extract();
}

}  // namespace

PasswordProtectionRequest::PasswordProtectionRequest(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    const std::string& mime_type,
    const std::string& username,
    PasswordType password_type,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    LoginReputationClientRequest::TriggerType type,
    bool password_field_exists,
    PasswordProtectionServiceBase* pps,
    int request_timeout_in_ms)
    : base::RefCountedDeleteOnSequence<PasswordProtectionRequest>(
          std::move(ui_task_runner)),
      request_proto_(std::make_unique<LoginReputationClientRequest>()),
      io_task_runner_(std::move(io_task_runner)),
      main_frame_url_(main_frame_url),
      password_form_action_(password_form_action),
      password_form_frame_url_(password_form_frame_url),
      mime_type_(mime_type),
      username_(username),
      password_type_(password_type),
      matching_domains_(GetMatchingDomains(matching_reused_credentials)),
      matching_reused_credentials_(matching_reused_credentials),
      trigger_type_(type),
      password_field_exists_(password_field_exists),
      password_protection_service_(pps),
      request_timeout_in_ms_(request_timeout_in_ms),
      is_modal_warning_showing_(false) {
  DCHECK(this->ui_task_runner()->RunsTasksInCurrentSequence());

  DCHECK(trigger_type_ == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
         trigger_type_ == LoginReputationClientRequest::PASSWORD_REUSE_EVENT);
  DCHECK(trigger_type_ != LoginReputationClientRequest::PASSWORD_REUSE_EVENT ||
         password_type_ != PasswordType::SAVED_PASSWORD ||
         !matching_reused_credentials_.empty());
  request_proto_->set_trigger_type(trigger_type_);
  *request_proto_->mutable_url_display_experiment() =
      pps->GetUrlDisplayExperiment();
}

PasswordProtectionRequest::~PasswordProtectionRequest() = default;

void PasswordProtectionRequest::Start() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  CheckAllowlist();
}

void PasswordProtectionRequest::CheckAllowlist() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // In order to send pings for about:blank, we skip the allowlist check for
  // URLs with unsupported schemes.
  if (!password_protection_service_->database_manager()->CanCheckUrl(
          main_frame_url_)) {
    OnAllowlistCheckDone(false);
    return;
  }

  // Start a task on the UI thread to check the allowlist. It may
  // callback immediately on the UI thread or take some time if a full-hash-
  // check is required.
  auto result_callback = base::BindOnce(
      &PasswordProtectionRequest::OnAllowlistCheckDone, AsWeakPtr());
  tracker_.PostTask(
      ui_task_runner().get(), FROM_HERE,
      base::BindOnce(&AllowlistCheckerClient::StartCheckCsdAllowlist,
                     password_protection_service_->database_manager(),
                     main_frame_url_, std::move(result_callback)));
}

void PasswordProtectionRequest::OnAllowlistCheckDone(bool match_allowlist) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  if (match_allowlist) {
    if (password_protection_service_->CanSendSamplePing()) {
      FillRequestProto(/*is_sampled_ping=*/true);
    }
    Finish(RequestOutcome::MATCHED_ALLOWLIST, nullptr);
  } else {
    // In case the request to Safe Browsing takes too long,
    // we set a timer to cancel that request and return an "unspecified verdict"
    // so that the navigation isn't blocked indefinitely.
    StartTimeout();
    CheckCachedVerdicts();
  }
}

void PasswordProtectionRequest::CheckCachedVerdicts() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
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

  password_protection_service_->FillUserPopulation(main_frame_url_,
                                                   request_proto_.get());
  // TODO(crbug.com/40918301): [Also TODO(thefrog)] Remove the
  // finch_active_groups modification below once kHashPrefixRealTimeLookups is
  // launched.
  const std::vector<const base::Feature*> kHashRealTimeLookupsFeature = {
      &kHashPrefixRealTimeLookups};
  GetExperimentStatus(kHashRealTimeLookupsFeature,
                      request_proto_->mutable_population());
  if (password_protection_service_->IsExtendedReporting() &&
      !password_protection_service_->IsIncognito()) {
    const std::vector<const base::Feature*> kAsyncChecksFeature = {
        &kSafeBrowsingAsyncRealTimeCheck};
    GetExperimentStatus(kAsyncChecksFeature,
                        request_proto_->mutable_population());
  }

  request_proto_->set_stored_verdict_cnt(
      password_protection_service_->GetStoredVerdictCount(trigger_type_));

  bool clicked_through_interstitial =
      password_protection_service_->UserClickedThroughSBInterstitial(this);
  request_proto_->set_clicked_through_interstitial(
      clicked_through_interstitial);
  request_proto_->set_content_type(*mime_type_);

#if BUILDFLAG(FULL_SAFE_BROWSING)
  if (password_protection_service_->IsExtendedReporting() &&
      !password_protection_service_->IsIncognito()) {
    gfx::Size content_area_size =
        password_protection_service_->GetCurrentContentAreaSize();
    request_proto_->set_content_area_height(content_area_size.height());
    request_proto_->set_content_area_width(content_area_size.width());
  }
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

#if BUILDFLAG(IS_ANDROID)
  SetReferringAppInfo();
#endif  // BUILDFLAG(IS_ANDROID)

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
      ReusedPasswordAccountType password_account_type_to_add =
          password_protection_service_
              ->GetPasswordProtectionReusedPasswordAccountType(password_type_,
                                                               username_);
      *reuse_event->mutable_reused_password_account_type() =
          password_account_type_to_add;
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (IsClientSideDetectionEnabled()) {
    GetDomFeatures();
  } else if (IsVisualFeaturesEnabled()) {
    MaybeCollectVisualFeatures();
  } else {
    SendRequest();
  }
#else
  SendRequest();
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
bool PasswordProtectionRequest::IsClientSideDetectionEnabled() {
  return false;
}

bool PasswordProtectionRequest::IsVisualFeaturesEnabled() {
  return false;
}
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

void PasswordProtectionRequest::SendRequest() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  if (password_protection_service_->CanGetAccessToken() &&
      password_protection_service_->token_fetcher()) {
    password_protection_service_->token_fetcher()->Start(base::BindOnce(
        &PasswordProtectionRequest::SendRequestWithToken, AsWeakPtr()));
    return;
  }
  std::string empty_access_token;
  SendRequestWithToken(empty_access_token);
}

void PasswordProtectionRequest::SendRequestWithToken(
    const std::string& access_token) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  MaybeAddPingToWebUI(access_token);

  std::string serialized_request;
  // TODO(crbug.com/40054172): Return early if request serialization fails.
  request_proto_->SerializeToString(&serialized_request);

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
  bool has_access_token = !access_token.empty();
  LogPasswordProtectionRequestTokenHistogram(trigger_type_, has_access_token);
  if (has_access_token) {
    LogAuthenticatedCookieResets(
        *resource_request,
        SafeBrowsingAuthenticatedEndpoint::kPasswordProtection);
    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token);
  }
  resource_request->url =
      PasswordProtectionServiceBase::GetPasswordProtectionRequestUrl();
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(serialized_request,
                                     "application/octet-stream");
  request_start_time_ = base::TimeTicks::Now();
  if (!prevent_initiating_url_loader_for_testing_) {
    url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        password_protection_service_->url_loader_factory().get(),
        base::BindOnce(&PasswordProtectionRequest::OnURLLoaderComplete,
                       AsWeakPtr()));
  }
}

void PasswordProtectionRequest::StartTimeout() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // If request is not done withing 10 seconds, we cancel this request.
  // The weak pointer used for the timeout will be invalidated (and
  // hence would prevent the timeout) if the check completes on time and
  // execution reaches Finish().
  ui_task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordProtectionRequest::Cancel, AsWeakPtr(), true),
      base::Milliseconds(request_timeout_in_ms_));
}

void PasswordProtectionRequest::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  const bool is_success = url_loader_->NetError() == net::OK;

  LogPasswordProtectionNetworkResponseAndDuration(
      response_code, url_loader_->NetError(), request_start_time_);

  if (!is_success || net::HTTP_OK != response_code) {
    Finish(RequestOutcome::FETCH_FAILED, nullptr);
    return;
  }

  std::unique_ptr<LoginReputationClientResponse> response =
      std::make_unique<LoginReputationClientResponse>();
  DCHECK(response_body);
  url_loader_.reset();  // We don't need it anymore.
  if (response_body && response->ParseFromString(*response_body)) {
    MaybeAddResponseToWebUI(*response);
    set_request_outcome(RequestOutcome::SUCCEEDED);
    Finish(RequestOutcome::SUCCEEDED, std::move(response));
  } else {
    Finish(RequestOutcome::RESPONSE_MALFORMED, nullptr);
  }
}

void PasswordProtectionRequest::Finish(
    RequestOutcome outcome,
    std::unique_ptr<LoginReputationClientResponse> response) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  tracker_.TryCancelAll();

  // If the request is canceled, the PasswordProtectionServiceBase is already
  // partially destroyed, and we won't be able to log accurate metrics.
  if (outcome != RequestOutcome::CANCELED) {
    ReusedPasswordAccountType password_account_type =
        password_protection_service_
            ->GetPasswordProtectionReusedPasswordAccountType(password_type_,
                                                             username_);
    if (trigger_type_ == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE) {
      LogPasswordOnFocusRequestOutcome(outcome);
    } else {
      LogPasswordEntryRequestOutcome(outcome, password_account_type);

      if (password_type_ == PasswordType::PRIMARY_ACCOUNT_PASSWORD) {
        MaybeLogPasswordReuseLookupEvent(outcome, response.get());
      }
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
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  url_loader_.reset();
  Finish(timed_out ? RequestOutcome::TIMEDOUT : RequestOutcome::CANCELED,
         nullptr);
}

}  // namespace safe_browsing
