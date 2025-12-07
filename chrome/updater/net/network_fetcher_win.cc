// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/network_fetcher.h"

#include <windows.h>

#include <iphlpapi.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/updater/event_logger.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/protos/omaha_usage_stats_event.pb.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/scoped_handle.h"
#include "chrome/updater/win/scoped_impersonation.h"
#include "chrome/updater/win/user_info.h"
#include "components/update_client/network.h"
#include "components/winhttp/proxy_configuration.h"
#include "components/winhttp/scoped_hinternet.h"
#include "url/gurl.h"

namespace updater {
namespace {

decltype(&GetNetworkConnectivityHint) GetGetNetworkConnectivityHint() {
  HMODULE hmod = LoadLibraryW(L"IPHLPAPI.DLL");
  if (!hmod) {
    return nullptr;
  }
  // GetNetworkConnectivityHint is not present on Windows < 19041 so this can
  // return nullptr on failure on older Windows versions.
  return reinterpret_cast<decltype(&GetNetworkConnectivityHint)>(
      GetProcAddress(hmod, "GetNetworkConnectivityHint"));
}

std::optional<int> GetNetworkConnectivityCostHint() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  static auto get_network_connectivity_hint_fn =
      GetGetNetworkConnectivityHint();
  if (!get_network_connectivity_hint_fn) {
    VLOG(1) << "Failed to lookup GetNetworkConnectivityHint";
    return std::nullopt;
  }

  NL_NETWORK_CONNECTIVITY_HINT connectivity_hint{
      .ConnectivityCost = ::NetworkConnectivityCostHintUnknown};
  NTSTATUS status = get_network_connectivity_hint_fn(&connectivity_hint);
  if (status != NO_ERROR) {
    VLOG(1) << "GetNetworkConnectivityHint failed with status " << status;
    return std::nullopt;
  }
  return static_cast<int>(connectivity_hint.ConnectivityCost);
}

// Factory method for the proxy configuration strategy.
scoped_refptr<winhttp::ProxyConfiguration> GetProxyConfiguration(
    std::optional<PolicyServiceProxyConfiguration>
        policy_service_proxy_configuration) {
  if (policy_service_proxy_configuration) {
    VLOG(1) << "Using cloud policy configuration for proxy.";
    return base::MakeRefCounted<winhttp::ProxyConfiguration>(winhttp::ProxyInfo{
        policy_service_proxy_configuration->proxy_auto_detect,
        base::SysUTF8ToWide(
            policy_service_proxy_configuration->proxy_pac_url.value_or("")),
        base::SysUTF8ToWide(
            policy_service_proxy_configuration->proxy_url.value_or("")),
        L""});
  }
  VLOG(1) << "Using the auto proxy configuration.";
  return base::MakeRefCounted<winhttp::AutoProxyConfiguration>();
}

class NetworkFetcher : public update_client::NetworkFetcher {
 public:
  using ResponseStartedCallback =
      ::update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = ::update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      ::update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      ::update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  NetworkFetcher(scoped_refptr<winhttp::SharedHInternet> session_handle,
                 scoped_refptr<winhttp::ProxyConfiguration> proxy_config,
                 proto::NetworkEvent_UserState user_state,
                 scoped_refptr<UpdaterEventLogger> event_logger);
  ~NetworkFetcher() override;
  NetworkFetcher(const NetworkFetcher&) = delete;
  NetworkFetcher& operator=(const NetworkFetcher&) = delete;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;
  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void PostRequestComplete(int response_code);
  void DownloadToFileComplete(int response_code);
  void LogEvent(int response_code);

  scoped_refptr<winhttp::NetworkFetcher> winhttp_network_fetcher_;
  scoped_refptr<UpdaterEventLogger> event_logger_;

  DownloadToFileCompleteCallback download_to_file_complete_callback_;
  PostRequestCompleteCallback post_request_complete_callback_;

  // Usage statistics.
  proto::NetworkEvent event_;
  base::Time request_start_time_;
  proto::NetworkEvent_UserState user_state_;
};

NetworkFetcher::NetworkFetcher(
    scoped_refptr<winhttp::SharedHInternet> session_handle,
    scoped_refptr<winhttp::ProxyConfiguration> proxy_config,
    proto::NetworkEvent_UserState user_state,
    scoped_refptr<UpdaterEventLogger> event_logger)
    : winhttp_network_fetcher_(
          base::MakeRefCounted<winhttp::NetworkFetcher>(session_handle,
                                                        proxy_config)),
      event_logger_(event_logger),
      user_state_(user_state) {
  event_.set_stack(proto::NetworkEvent::DIRECT);
}

NetworkFetcher::~NetworkFetcher() {
  winhttp_network_fetcher_->Close();
}

void NetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;
  post_request_complete_callback_ = std::move(post_request_complete_callback);
  event_.set_url(url.spec());
  request_start_time_ = base::Time::Now();
  winhttp_network_fetcher_->PostRequest(
      url, post_data, content_type, post_additional_headers,
      std::move(response_started_callback), std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::PostRequestComplete,
                     base::Unretained(this)));
}

base::OnceClosure NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;
  download_to_file_complete_callback_ =
      std::move(download_to_file_complete_callback);
  event_.set_url(url.spec());
  request_start_time_ = base::Time::Now();
  return winhttp_network_fetcher_->DownloadToFile(
      url, file_path, std::move(response_started_callback),
      std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::DownloadToFileComplete,
                     base::Unretained(this)));
}

void NetworkFetcher::PostRequestComplete(int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__ << ": response code=" << response_code;

  // Attempt to get some response headers.  Not all headers may be present so
  // this is best effort only.
  std::wstring x_cup_server_proof;
  std::wstring etag;
  std::wstring set_cookie;
  int x_retry_after_sec = -1;
  winhttp_network_fetcher_->QueryHeaderString(
      base::SysUTF8ToWide(
          update_client::NetworkFetcher::kHeaderXCupServerProof),
      &x_cup_server_proof);
  winhttp_network_fetcher_->QueryHeaderString(
      base::SysUTF8ToWide(update_client::NetworkFetcher::kHeaderEtag), &etag);
  winhttp_network_fetcher_->QueryHeaderString(
      base::SysUTF8ToWide(update_client::NetworkFetcher::kHeaderSetCookie),
      &set_cookie);
  winhttp_network_fetcher_->QueryHeaderInt(
      base::SysUTF8ToWide(update_client::NetworkFetcher::kHeaderXRetryAfter),
      &x_retry_after_sec);

  LogEvent(response_code);

  std::move(post_request_complete_callback_)
      .Run(winhttp_network_fetcher_->GetResponseBody(),
           winhttp_network_fetcher_->GetNetError(), base::SysWideToUTF8(etag),
           base::SysWideToUTF8(x_cup_server_proof),
           base::SysWideToUTF8(set_cookie), x_retry_after_sec);
}

void NetworkFetcher::DownloadToFileComplete(int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;
  LogEvent(response_code);
  std::move(download_to_file_complete_callback_)
      .Run(winhttp_network_fetcher_->GetNetError(),
           winhttp_network_fetcher_->GetContentSize());
}

void NetworkFetcher::LogEvent(int response_code) {
  if (!event_logger_) {
    return;
  }

  event_.set_bytes_received(winhttp_network_fetcher_->GetContentSize());
  event_.set_elapsed_time_ms(
      (base::Time::Now() - request_start_time_).InMilliseconds());
  const HRESULT net_error = winhttp_network_fetcher_->GetNetError();
  if (FAILED(net_error)) {
    event_.set_error_code(net_error);
  } else if (response_code < 200 || response_code > 299) {
    event_.set_error_code(response_code);
  }

  if (user_state_ != proto::NetworkEvent_UserState_UNSPECIFIED) {
    event_.set_user_state(user_state_);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce([] { return GetNetworkConnectivityCostHint(); }),
      base::BindOnce(
          [](scoped_refptr<UpdaterEventLogger> event_logger,
             proto::NetworkEvent event, std::optional<int> connection_cost) {
            if (connection_cost) {
              event.set_connection_cost(*connection_cost);
            }

            proto::Omaha4Metric metric;
            *metric.mutable_network_event() = std::move(event);
            event_logger->Log(std::move(metric));
          },
          event_logger_, std::move(event_)));
}

}  // namespace

class NetworkFetcherFactory::Impl {
 public:
  explicit Impl(std::optional<PolicyServiceProxyConfiguration>
                    policy_service_proxy_configuration,
                scoped_refptr<UpdaterEventLogger> event_logger)
      : proxy_configuration_(
            GetProxyConfiguration(policy_service_proxy_configuration)),
        event_logger_(event_logger) {
    ScopedImpersonation impersonate;
    if (IsSystemInstall()) {
      user_state_ = proto::NetworkEvent_UserState_NOT_LOGGED_IN;
      HResultOr<ScopedKernelHANDLE> token = GetLoggedOnUserToken();
      VLOG_IF(2, !token.has_value())
          << __func__ << ": GetLoggedOnUserToken failed: " << std::hex
          << token.error();
      if (token.has_value()) {
        const HRESULT hr = impersonate.Impersonate(token.value().get());
        VLOG(2)
            << __func__
            << ": Successfully got logged on user token. Impersonate result: "
            << std::hex << hr;
        if (SUCCEEDED(hr)) {
          user_state_ = proto::NetworkEvent_UserState_LOGGED_IN;
        }
      }
    } else {
      user_state_ = proto::NetworkEvent_UserState_LOGGED_IN;
    }
    session_handle_ = base::MakeRefCounted<winhttp::SharedHInternet>(
        winhttp::CreateSessionHandle(base::SysUTF8ToWide(GetUpdaterUserAgent()),
                                     proxy_configuration_->access_type(),
                                     proxy_configuration_->proxy(),
                                     proxy_configuration_->proxy_bypass()));
    VLOG_IF(2, !session_handle_) << "Failed to create a winhttp session.";
  }

  std::unique_ptr<update_client::NetworkFetcher> Create() {
    return session_handle_ ? std::make_unique<NetworkFetcher>(
                                 session_handle_, proxy_configuration_,
                                 user_state_, event_logger_)
                           : nullptr;
  }

 private:
  scoped_refptr<winhttp::ProxyConfiguration> proxy_configuration_;
  scoped_refptr<UpdaterEventLogger> event_logger_;
  scoped_refptr<winhttp::SharedHInternet> session_handle_;
  proto::NetworkEvent_UserState user_state_;
};

NetworkFetcherFactory::NetworkFetcherFactory(
    std::optional<PolicyServiceProxyConfiguration>
        policy_service_proxy_configuration,
    scoped_refptr<UpdaterEventLogger> event_logger)
    : impl_(std::make_unique<Impl>(policy_service_proxy_configuration,
                                   event_logger)) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<LoggingNetworkFetcher>(impl_->Create());
}

}  // namespace updater
