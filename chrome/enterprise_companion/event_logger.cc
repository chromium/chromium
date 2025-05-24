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
#include "base/command_line.h"
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
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/flags.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "chrome/enterprise_companion/telemetry_logger/proto/log_request.pb.h"
#include "chrome/enterprise_companion/telemetry_logger/telemetry_logger.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_inclusion_status.h"
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

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace enterprise_companion {

namespace {

using HttpRequestCallback =
    base::OnceCallback<void(std::optional<int> http_status,
                            std::optional<std::string> response_body)>;

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

proto::EnterpriseCompanionMetadata GetMetadata() {
  proto::EnterpriseCompanionMetadata metadata;
  metadata.set_app_version(kEnterpriseCompanionVersion);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kCohortIdSwitch)) {
    metadata.set_omaha_cohort_id(
        command_line->GetSwitchValueASCII(kCohortIdSwitch));
  }

#if defined(ARCH_CPU_X86)
  metadata.set_application_arch(proto::X86);
#elif defined(ARCH_CPU_X86_64)
  metadata.set_application_arch(proto::X86_64);
#elif defined(ARCH_CPU_ARM64)
  metadata.set_application_arch(proto::ARM64);
#endif

#if BUILDFLAG(IS_LINUX)
  metadata.set_os_platform(proto::LINUX);
#elif BUILDFLAG(IS_MAC)
  metadata.set_os_platform(proto::MAC);
#elif BUILDFLAG(IS_WIN)
  metadata.set_os_platform(proto::WINDOWS);
#endif

#if BUILDFLAG(IS_WIN)
  const base::win::OSInfo::VersionNumber v =
      base::win::OSInfo::GetInstance()->version_number();
  metadata.set_os_version(
      base::StringPrintf("%u.%u.%u", v.major, v.minor, v.build));
#elif BUILDFLAG(IS_MAC)
  int32_t os_major = 0, os_minor = 0, os_bugfix = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major, &os_minor,
                                               &os_bugfix);
  metadata.set_os_version(base::StringPrintf("%u.%u", os_major, os_minor));
#endif

  const std::string os_arch = base::SysInfo::OperatingSystemArchitecture();
  if (os_arch == "x86") {
    metadata.set_os_arch(proto::X86);
  } else if (os_arch == "x86_64") {
    metadata.set_os_arch(proto::X86_64);
  } else if (os_arch == "arm64") {
    metadata.set_os_arch(proto::ARM64);
  }

  return metadata;
}

proto::Status ToProtoStatus(EnterpriseCompanionStatus status) {
  proto::Status proto_status;
  proto_status.set_space(status.space());
  proto_status.set_code(status.code());
  return proto_status;
}

// Creates a logging cookie from the provided value. Returns nullptr if the
// value is invalid.
std::unique_ptr<net::CanonicalCookie> MakeLoggingCookieFromValue(
    const GURL& url,
    const std::string& cookie_value) {
  net::CookieInclusionStatus cookie_status;
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url, "NID", cookie_value,
          base::StrCat({".", url::SchemeHostPort(url).host()}),
          /*path=*/"/", /*creation_time=*/base::Time::Now(),
          /*expiration_time=*/base::Time::Now() + base::Days(180),
          /*last_access_time=*/base::Time::Now(), /*secure=*/false,
          /*http_only=*/true, net::CookieSameSite::UNSPECIFIED,
          net::CookiePriority::COOKIE_PRIORITY_DEFAULT,
          /*partition_key=*/std::nullopt, &cookie_status);
  if (!cookie) {
    VLOG(1) << "Failed to initialize logging cookie: " << cookie_status;
    return nullptr;
  }
  if (!cookie->IsCanonical()) {
    VLOG(1) << "Failed to initialize logging cookie. Not canonical: "
            << cookie->DebugString();
    return nullptr;
  }
  return cookie;
}

// Initializes a logging cookie from the contents of a file. Returns nullptr if
// the file can not be read or the contents are invalid.
std::unique_ptr<net::CanonicalCookie> MakeLoggingCookieFromFile(
    const GURL& url,
    base::File& file) {
  std::string cookie_value(kCookieValueBufferSize, 0);
  std::optional<size_t> bytes_read =
      file.Read(0, base::as_writable_byte_span(cookie_value));
  if (!bytes_read || bytes_read == 0) {
    return nullptr;
  }
  cookie_value.resize(*bytes_read);
  return MakeLoggingCookieFromValue(url, cookie_value);
}

// Initializes a logging cookie from the contents of a file. If the file could
// not be read or the contents are invalid, returns a logging cookie with the
// default value. Returns nullptr if no cookie could be made.
std::unique_ptr<net::CanonicalCookie> MakeLoggingCookie(const GURL& url,
                                                        base::File& file) {
  std::unique_ptr<net::CanonicalCookie> file_cookie =
      MakeLoggingCookieFromFile(url, file);
  return file_cookie
             ? std::move(file_cookie)
             : MakeLoggingCookieFromValue(url, kLoggingCookieDefaultValue);
}

class EventUploader {
 public:
  explicit EventUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory) {}

  void DoPostRequest(const GURL& url,
                     const std::string& request_body,
                     HttpRequestCallback callback) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->request_initiator = url::Origin::Create(url);
    resource_request->site_for_cookies = net::SiteForCookies::FromUrl(url);
    resource_request->method = net::HttpRequestHeaders::kPostMethod;

    std::unique_ptr<network::SimpleURLLoader> url_loader =
        network::SimpleURLLoader::Create(std::move(resource_request),
                                         kTrafficAnnotation);
    url_loader->SetAllowHttpErrorResults(true);
    url_loader->AttachStringForUpload(request_body);
    url_loader->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&EventUploader::OnLogResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                       std::move(callback)),
        1024 * 1024 /* 1 MiB */);
  }

  virtual ~EventUploader() {
    VLOG(1) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  void OnLogResponseReceived(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      HttpRequestCallback callback,
      std::optional<std::string> response_body) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (url_loader->NetError() != net::OK) {
      VLOG(1) << "Logging request failed "
              << net::ErrorToString(url_loader->NetError());
    }

    network::mojom::URLResponseHeadPtr response_head =
        url_loader->TakeResponseInfo();
    std::optional<int> http_status;
    if (response_head && response_head.get()->headers) {
      http_status = response_head.get()->headers->response_code();
    }
    std::move(callback).Run(http_status, response_body);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<EventUploader> weak_ptr_factory_{this};
};

class EventLoggerDelegate : public EventTelemetryLogger::Delegate {
 public:
  explicit EventLoggerDelegate(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : uploader_(base::SequenceBound<EventUploader>(
            base::SequencedTaskRunner::GetCurrentDefault(),
            url_loader_factory)) {}

  // Overrides of EventLogger:Delegate.
  // This is a long-live app and doesn't actually store the next allowed time
  // for relaunch.
  bool StoreNextAllowedAttemptTime(base::Time time) override { return true; }
  std::optional<base::Time> GetNextAllowedAttemptTime() const override {
    return std::nullopt;
  }

  void DoPostRequest(const std::string& request_body,
                     HttpRequestCallback callback) override {
    uploader_.AsyncCall(&EventUploader::DoPostRequest)
        .WithArgs(GetGlobalConstants()->EnterpriseCompanionEventLoggingURL(),
                  request_body,
                  base::BindPostTaskToCurrentDefault(std::move(callback)));
  }

  int GetLogIdentifier() const override {
    return telemetry_logger::proto::CHROME_ENTERPRISE_COMPANION_APP;
  }

  std::string AggregateAndSerializeEvents(
      base::span<proto::EnterpriseCompanionEvent> events) const override {
    proto::ChromeEnterpriseCompanionAppExtension extension;
    *extension.mutable_metadata() = GetMetadata();
    for (const proto::EnterpriseCompanionEvent& event : events) {
      *extension.add_event() = event;
    }
    return extension.SerializeAsString();
  }

  base::TimeDelta MinimumCooldownTime() const override {
    return GetGlobalConstants()->EventLoggerMinTimeout();
  }

 private:
  base::SequenceBound<EventUploader> uploader_;
  base::WeakPtrFactory<EventLoggerDelegate> weak_ptr_factory_{this};
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
    const GURL event_logging_url =
        GetGlobalConstants()->EnterpriseCompanionEventLoggingURL();
    std::unique_ptr<net::CanonicalCookie> cookie =
        MakeLoggingCookie(event_logging_url, cookie_file_);
    if (!cookie) {
      VLOG(1) << "Failed to initialize logging cookie.";
      std::move(callback).Run();
      return;
    }

    net::CookieOptions cookie_options;
    cookie_options.set_include_httponly();
    cookie_options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext(
            net::CookieOptions::SameSiteCookieContext::ContextType::
                SAME_SITE_STRICT));
    cookie_manager_remote_->SetCanonicalCookie(
        *cookie, event_logging_url, cookie_options,
        base::BindOnce([](net::CookieAccessResult result) {
          LOG_IF(ERROR, !result.status.IsInclude())
              << "Failed to set logging cookie: " << result.status;
        }).Then(std::move(callback)));
  }

  // Overrides for network::mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (change.cause == net::CookieChangeCause::INSERTED) {
      if (!cookie_file_.WriteAndCheck(
              0, base::as_byte_span(change.cookie.Value())) ||
          !cookie_file_.SetLength(change.cookie.Value().length())) {
        VLOG(1) << "Failed to write logging cookie: "
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

class EnterpriseCompanionEventLoggerImpl
    : public EnterpriseCompanionEventLogger {
 public:
  explicit EnterpriseCompanionEventLoggerImpl(
      std::unique_ptr<EventTelemetryLogger::Delegate> logger_delegate)
      : impl_(EventTelemetryLogger::Create(std::move(logger_delegate))) {}
  EnterpriseCompanionEventLoggerImpl() = delete;
  EnterpriseCompanionEventLoggerImpl(
      const EnterpriseCompanionEventLoggerImpl&) = delete;
  EnterpriseCompanionEventLoggerImpl& operator=(
      const EnterpriseCompanionEventLoggerImpl) = delete;
  void LogRegisterPolicyAgentEvent(
      base::Time start_time,
      StatusCallback callback,
      const EnterpriseCompanionStatus& status) override {
    VLOG(2) << __func__ << ": status=" << status.description();
    proto::EnterpriseCompanionEvent event;
    *event.mutable_status() = ToProtoStatus(status);
    event.set_duration_ms((base::Time::Now() - start_time).InMilliseconds());
    *event.mutable_browser_enrollment_event() = proto::BrowserEnrollmentEvent();
    impl_->Log(event);
    std::move(callback).Run(status);
  }
  void LogPolicyFetchEvent(base::Time start_time,
                           StatusCallback callback,
                           const EnterpriseCompanionStatus& status) override {
    VLOG(2) << __func__ << ": status=" << status.description();
    proto::EnterpriseCompanionEvent event;
    *event.mutable_status() = ToProtoStatus(status);
    event.set_duration_ms((base::Time::Now() - start_time).InMilliseconds());
    *event.mutable_policy_fetch_event() = proto::PolicyFetchEvent();
    impl_->Log(event);
    std::move(callback).Run(status);
  }
  void Flush(base::OnceClosure callback) override {
    return impl_->Flush(std::move(callback));
  }

 protected:
  friend class base::RefCountedThreadSafe<EnterpriseCompanionEventLogger>;
  ~EnterpriseCompanionEventLoggerImpl() override = default;

 private:
  scoped_refptr<EventTelemetryLogger> impl_;
};

}  // namespace

const char kLoggingCookieName[] = "NID";
const char kLoggingCookieDefaultValue[] = "\"\"";

std::optional<base::File> OpenDefaultEventLoggerCookieFile() {
  std::optional<base::FilePath> install_dir = GetInstallDirectory();
  if (!install_dir) {
    return std::nullopt;
  }
  return base::File(install_dir->Append(FILE_PATH_LITERAL("logging_cookie")),
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

scoped_refptr<EnterpriseCompanionEventLogger>
EnterpriseCompanionEventLogger::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return base::MakeRefCounted<EnterpriseCompanionEventLoggerImpl>(
      std::make_unique<EventLoggerDelegate>(url_loader_factory));
}

}  // namespace enterprise_companion
