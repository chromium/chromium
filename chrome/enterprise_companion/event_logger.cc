// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/event_logger.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "chrome/enterprise_companion/proto/log_request.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_companion {

namespace {

constexpr size_t kCookieValueBufferSize = 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("enterprise_companion_event_logging",
                                        R"(
        semantics {
          sender: "Chrome Enterprise Companion App"
          description:
            "Service logging for the Chrome Enterprise Companion App."
          trigger: "Periodic tasks."
          data: "Metrics about the Chrome Enterprise Companion App."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "noahrose@google.com"
            }
            contacts {
              email: "chrome-updates-dev@chromium.org"
            }
          }
          last_reviewed: "2024-07-08"
          user_data {
            type: NONE
          }
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Chrome Enterprise Companion App cookie store"
          setting:
            "This feature cannot be disabled other than by uninstalling the "
            "Chrome Enterprise Companion App."
          policy_exception_justification:
            "This request is made by the Chrome Enterprise Companion App, not "
            "Chrome itself."
        })");

// An individual event logger. Events are locally batched and flushed to the
// manager, which performs global batching.
class EventLoggerImpl : public EventLogger {
 public:
  using EventConsumerCallback = base::RepeatingCallback<void(
      std::vector<proto::EnterpriseCompanionEvent> events)>;

  EventLoggerImpl(EventConsumerCallback event_consumer,
                  const base::Clock* clock)
      : event_consumer_(event_consumer), clock_(clock) {}

  void Flush() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    event_consumer_.Run(std::move(events_));
  }

  OnEnrollmentFinishCallback OnEnrollmentStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::BindPostTaskToCurrentDefault(
        base::BindOnce(&EventLoggerImpl::OnEnrollmentFinish,
                       base::WrapRefCounted(this), clock_->Now()));
  }

  OnPolicyFetchFinishCallback OnPolicyFetchStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::BindPostTaskToCurrentDefault(
        base::BindOnce(&EventLoggerImpl::OnPolicyFetchFinish,
                       base::WrapRefCounted(this), clock_->Now()));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  EventConsumerCallback event_consumer_;
  const raw_ptr<const base::Clock> clock_;
  std::vector<proto::EnterpriseCompanionEvent> events_;

  void OnEnrollmentFinish(base::Time start_time,
                          const EnterpriseCompanionStatus& status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::TimeDelta duration = clock_->Now() - start_time;

    proto::EnterpriseCompanionEvent event;
    *event.mutable_status() = status.ToProtoStatus();
    event.set_duration_ms(duration.InMilliseconds());
    *event.mutable_browser_enrollment_event() = proto::BrowserEnrollmentEvent();

    events_.push_back(std::move(event));
  }

  void OnPolicyFetchFinish(base::Time start_time,
                           const EnterpriseCompanionStatus& status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::TimeDelta duration = clock_->Now() - start_time;

    proto::EnterpriseCompanionEvent event;
    *event.mutable_status() = status.ToProtoStatus();
    event.set_duration_ms(duration.InMilliseconds());
    *event.mutable_policy_fetch_event() = proto::PolicyFetchEvent();

    events_.push_back(std::move(event));
  }

 private:
  ~EventLoggerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Flush();
  }
};

class EventLogUploaderImpl : public EventLogUploader {
 public:
  explicit EventLogUploaderImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory) {}

  ~EventLogUploaderImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void DoLogRequest(proto::LogRequest request,
                    LogRequestCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!url_loader_) << "Overlapping log requests are not permitted.";

    const GURL event_logging_url =
        GetGlobalConstants()->EnterpriseCompanionEventLoggingURL();

    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->url = event_logging_url;
    resource_request->request_initiator =
        url::Origin::Create(event_logging_url);
    resource_request->site_for_cookies =
        net::SiteForCookies::FromUrl(event_logging_url);
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   kTrafficAnnotation);
    url_loader_->SetAllowHttpErrorResults(true);
    url_loader_->AttachStringForUpload(request.SerializeAsString());
    url_loader_->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&EventLogUploaderImpl::OnLogResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        1024 * 1024 /* 1 MiB */);
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  void OnLogResponseReceived(LogRequestCallback callback,
                             std::optional<std::string> response_body) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::unique_ptr<network::SimpleURLLoader> url_loader =
        std::move(url_loader_);
    CHECK(url_loader);

    if (url_loader->NetError() != net::OK) {
      LOG(ERROR) << "Logging request failed "
                 << net::ErrorToString(url_loader->NetError());
    }

    std::move(callback).Run(url_loader->TakeResponseInfo(),
                            std::move(response_body));
  }

  base::WeakPtrFactory<EventLogUploaderImpl> weak_ptr_factory_{this};
};

class EventLoggerManagerImpl : public EventLoggerManager {
 public:
  EventLoggerManagerImpl(const base::Clock* clock,
                         std::unique_ptr<EventLogUploader> uploader)
      : clock_(clock), uploader_(std::move(uploader)) {}

  ~EventLoggerManagerImpl() override = default;

  scoped_refptr<EventLogger> CreateEventLogger() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::MakeRefCounted<EventLoggerImpl>(
        base::BindRepeating(&EventLoggerManagerImpl::AcceptLogs,
                            weak_ptr_factory_.GetWeakPtr()),
        clock_);
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<const base::Clock> clock_;
  std::unique_ptr<EventLogUploader> uploader_;
  bool can_make_request_ = true;
  std::vector<proto::EnterpriseCompanionEvent> events_;
  base::OneShotTimer cooldown_timer_;

  // Called by EventLoggers to ingest a batch of logs. If not rate-limited, this
  // will synchronously trigger a transmission. Otherwise, the logs are queued
  // to be uploaded.
  void AcceptLogs(std::vector<proto::EnterpriseCompanionEvent> events) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    events_.insert(events_.end(), std::make_move_iterator(events.begin()),
                   std::make_move_iterator(events.end()));

    if (can_make_request_) {
      Transmit();
    }
  }

  // Transmits logs to the remote endpoint, if there are any. Schedules the
  // next transmission using the timeout provided by the server.
  void Transmit() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (events_.empty()) {
      return;
    }

    can_make_request_ = false;

    int64_t now_ms = clock_->Now().InMillisecondsSinceUnixEpoch();
    proto::LogRequest request;
    request.set_request_time_ms(now_ms);
    request.mutable_client_info()->set_client_type(
        proto::ClientInfo_ClientType_CHROME_ENTERPRISE_COMPANION);
    request.set_log_source(proto::CHROME_ENTERPRISE_COMPANION_APP);
    proto::ChromeEnterpriseCompanionAppExtension extension;
    for (const proto::EnterpriseCompanionEvent& event : events_) {
      *extension.add_event() = event;
    }
    proto::LogEvent* log_event = request.add_log_event();
    log_event->set_event_time_ms(now_ms);
    log_event->set_source_extension(extension.SerializeAsString());

    uploader_->DoLogRequest(
        std::move(request),
        base::BindOnce(&EventLoggerManagerImpl::OnLogResponseReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnLogResponseReceived(
      mojo::StructPtr<network::mojom::URLResponseHead> response_info,
      std::optional<std::string> response_body) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (ShouldDeleteEvents(response_info.get())) {
      events_.clear();
    }

    const base::TimeDelta min_timeout =
        GetGlobalConstants()->EventLoggerMinTimeout();

    if (!response_info) {
      SetCooldown(min_timeout);
      return;
    }

    proto::LogResponse response;
    if (!response.ParseFromString(*response_body)) {
      LOG(ERROR) << "Failed to parse log response proto";
      if (response_info->mime_type != "text/plain") {
        LOG(ERROR) << "Log response: " << *response_body;
      }
      SetCooldown(min_timeout);
      return;
    }

    SetCooldown(std::max(
        base::Milliseconds(response.next_request_wait_millis()), min_timeout));
  }

  void SetCooldown(base::TimeDelta cooldown) {
    cooldown_timer_.Start(
        FROM_HERE, cooldown,
        base::BindOnce(&EventLoggerManagerImpl::OnCooldownExhausted,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnCooldownExhausted() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    can_make_request_ = true;
    if (!events_.empty()) {
      Transmit();
    }
  }

  bool ShouldDeleteEvents(
      const network::mojom::URLResponseHead* response_info) {
    if (!response_info || !response_info->headers) {
      return false;
    }
    int response_code = response_info->headers->response_code();
    // Delete logs for the 2xx and 4xx family of responses.
    return (response_code >= 200 && response_code < 300) ||
           (response_code >= 400 && response_code < 500);
  }

  base::WeakPtrFactory<EventLoggerManagerImpl> weak_ptr_factory_{this};
};

class EventLoggerCookieHandlerImpl : public EventLoggerCookieHandler,
                                     network::mojom::CookieChangeListener {
 public:
  explicit EventLoggerCookieHandlerImpl(base::File cookie_file)
      : cookie_file_(std::move(cookie_file)) {}

  ~EventLoggerCookieHandlerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Init(mojo::PendingRemote<network::mojom::CookieManager>
                cookie_manager_pending_remote,
            base::OnceClosure callback) override {
    cookie_manager_remote_ =
        mojo::Remote(std::move(cookie_manager_pending_remote));
    cookie_manager_remote_->AddCookieChangeListener(
        GetGlobalConstants()->EnterpriseCompanionEventLoggingURL(),
        kLoggingCookieName,
        cookie_listener_receiver_.BindNewPipeAndPassRemote());
    InitLoggingCookie(std::move(callback));
  }

 private:
  // Adds the persisted event logging cookie to the store, if one is present.
  // Otherwise, adds an empty cookie, prompting the server to provision a new
  // one on the next request.
  void InitLoggingCookie(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::string cookie_value(kCookieValueBufferSize, 0);
    std::optional<size_t> bytes_read =
        cookie_file_.Read(0, base::as_writable_byte_span(cookie_value));
    if (!bytes_read || bytes_read == 0) {
      // If no logging cookie is present, the default value will signal to the
      // server to provision a new one.
      cookie_value = kLoggingCookieDefaultValue;
    } else {
      cookie_value.resize(*bytes_read);
    }

    const GURL event_logging_url =
        GetGlobalConstants()->EnterpriseCompanionEventLoggingURL();

    net::CookieOptions cookie_options;
    cookie_options.set_include_httponly();
    cookie_options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext(
            net::CookieOptions::SameSiteCookieContext::ContextType::
                SAME_SITE_STRICT));
    url::SchemeHostPort event_logging_scheme_host_port =
        url::SchemeHostPort(event_logging_url);
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateSanitizedCookie(
            event_logging_url, "NID", cookie_value,
            base::StrCat({".", event_logging_scheme_host_port.host()}),
            /*path=*/"/", /*creation_time=*/base::Time::Now(),
            /*expiration_time=*/base::Time::Now() + base::Days(180),
            /*last_access_time=*/base::Time::Now(), /*secure=*/false,
            /*http_only=*/true, net::CookieSameSite::UNSPECIFIED,
            net::CookiePriority::COOKIE_PRIORITY_DEFAULT,
            /*partition_key=*/std::nullopt, /*status=*/nullptr);

    if (cookie->IsCanonical()) {
      cookie_manager_remote_->SetCanonicalCookie(
          *cookie, event_logging_url, cookie_options,
          base::BindOnce([](net::CookieAccessResult result) {
            LOG_IF(ERROR, !result.status.IsInclude())
                << "Failed to set logging cookie: " << result.status;
          }).Then(std::move(callback)));
    } else {
      LOG(ERROR) << "Failed to initialize logging cookie. Not canonical: "
                 << cookie->DebugString();
      std::move(callback).Run();
    }
  }

  // Overrides for network::mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (change.cause == net::CookieChangeCause::INSERTED) {
      if (!cookie_file_.WriteAndCheck(
              0, base::as_bytes(base::make_span(change.cookie.Value()))) ||
          !cookie_file_.SetLength(change.cookie.Value().length())) {
        LOG(ERROR) << "Failed to write logging cookie: "
                   << change.cookie.DebugString();
      }
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::File cookie_file_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;
  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};
};

}  // namespace

const char kLoggingCookieName[] = "NID";
const char kLoggingCookieDefaultValue[] = "\"\"";

std::unique_ptr<EventLoggerManager> CreateEventLoggerManager(
    std::unique_ptr<EventLogUploader> uploader,
    const base::Clock* clock) {
  return std::make_unique<EventLoggerManagerImpl>(clock, std::move(uploader));
}

std::unique_ptr<EventLogUploader> CreateEventLogUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<EventLogUploaderImpl>(std::move(url_loader_factory));
}

std::optional<base::File> OpenDefaultEventLoggerCookieFile() {
  std::optional<base::FilePath> install_dir = GetInstallDirectory();
  if (!install_dir) {
    return std::nullopt;
  }
  return base::File(install_dir->AppendASCII("logging_cookie"),
                    base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                        base::File::FLAG_WRITE);
}

base::SequenceBound<EventLoggerCookieHandler> CreateEventLoggerCookieHandler(
    std::optional<base::File> logging_cookie_file) {
  if (!logging_cookie_file || !logging_cookie_file->IsValid()) {
    return {};
  }
  return base::SequenceBound<EventLoggerCookieHandlerImpl>(
      base::SequencedTaskRunner::GetCurrentDefault(),
      std::move(*logging_cookie_file));
}

}  // namespace enterprise_companion
