// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/id_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/version_info/channel.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/api_access_token_fetcher.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto/permissions_common.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher_status.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "net/base/request_priority.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace supervised_user {
// -----------------------------------------------------------------------------
// Usage documentation
// -----------------------------------------------------------------------------
//
// Overview: ProtoFetcher provides an interface for generic fetchers that
// use classes to represent Request and Response objects. The default mechanism
// under the hood takes care of the fetch process, including:
// * obtaining the right access token,
// * serializing the request and parsing the response,
// * submitting metrics.
//
// If you want to create new fetcher factory method, then some
// details must be provided in order to enable fetching for said Response. The
// new fetcher factory should have at least the following arguments:
// signin::IdentityManager, network::SharedURLLoaderFactory, consuming callback
// and must reference a static configuration.
//
// The static configuration should be placed in the fetcher_config.h module.

// Encapsulates metric functionalities.
class Metrics {
 public:
  enum class MetricType {
    kStatus,
    kLatency,
    kHttpStatusOrNetError,
    kRetryCount,
    kAuthError,
  };

  Metrics() = delete;
  static std::optional<Metrics> FromConfig(const FetcherConfig& config);

  void RecordStatus(const ProtoFetcherStatus& status) const;
  void RecordLatency() const;
  void RecordAccessTokenLatency(GoogleServiceAuthError::State auth_error_state);
  void RecordApiLatency(
      ProtoFetcherStatus::HttpStatusOrNetErrorType http_status_or_net_error);
  virtual void RecordStatusLatency(const ProtoFetcherStatus& status) const;
  void RecordAuthError(const GoogleServiceAuthError& auth_error) const;
  void RecordHttpStatusOrNetError(const ProtoFetcherStatus& status) const;

 protected:
  // Translates top-level metric type into a string. ::ToMetricEnumLabel
  // translates statuses for per-status latency tracking.
  virtual std::string GetMetricKey(MetricType metric_type) const;

  // Returns fully-qualified name of histogram for specified metric_type.
  std::string GetFullHistogramName(MetricType metric_type) const;

  // Returns fully-qualified name of histogram for specified metric_type with
  // per-status values.
  std::string GetFullHistogramName(MetricType metric_type,
                                   ProtoFetcherStatus status) const;

  // Returns fully-qualified name of histogram for specified metric_type with
  // per-authentication status values.
  std::string GetFullHistogramName(
      MetricType metric_type,
      GoogleServiceAuthError::State auth_error_state) const;

  // Returns fully-qualified name of histogram for specified metric_type with
  // per-net-or-http error values.
  std::string GetFullHistogramName(MetricType metric_type,
                                   ProtoFetcherStatus::HttpStatusOrNetErrorType
                                       http_status_or_net_error) const;

 protected:
  explicit Metrics(std::string_view basename);

 private:
  // The returned value must match one of the labels in
  // chromium/src/tools/metrics/histograms/enums.xml://enum[@name='ProtoFetcherStatus'],
  // and should be reflected in tokens in histogram defined for this fetcher.
  // See example at
  // tools/metrics/histograms/metadata/signin/histograms.xml://histogram[@name='Signin.ListFamilyMembersRequest.{Status}.*']
  static std::string ToMetricEnumLabel(const ProtoFetcherStatus& status);

  std::string_view basename_;
  base::ElapsedTimer elapsed_timer_;
};

// Metrics for retrying fetchers, which are aggregating individual
// fetchers.
class OverallMetrics final : public Metrics {
 public:
  OverallMetrics() = delete;
  static std::optional<OverallMetrics> FromConfig(const FetcherConfig& config);

  // Per-status latency is not defined for OverallMetrics.
  void RecordStatusLatency(const ProtoFetcherStatus& status) const override;
  void RecordRetryCount(int count) const;

 protected:
  std::string GetMetricKey(MetricType metric_type) const override;

 private:
  explicit OverallMetrics(std::string_view basename);
};

// Uses network::SharedURLLoaderFactory to issue network requests.
// Internally, it's a two-phase process: first the access token is fetched, and
// if applicable, the remote service is called and the response is processed.
// This abstract class doesn't make any assumptions on the request nor response
// formats and uses them as bare strings.
class FetchProcess {
 public:
  FetchProcess() = delete;

  // Identity manager and fetcher_config must outlive this call.
  FetchProcess(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view payload,
      const FetcherConfig& fetcher_config,
      const FetcherConfig::PathArgs& args = {},
      std::optional<version_info::Channel> channel = std::nullopt);

  // Not copyable.
  FetchProcess(const FetchProcess&) = delete;
  FetchProcess& operator=(const FetchProcess&) = delete;

  virtual ~FetchProcess();
  bool IsMetricsRecordingEnabled() const;

 protected:
  void RecordMetrics(const ProtoFetcherStatus& status);

 private:
  // First phase of fetching: the access token response is ready.
  void OnAccessTokenFetchComplete(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
          access_token);
  // Second phase of fetching: the remote service responded.
  void OnSimpleUrlLoaderComplete(std::unique_ptr<std::string> response_body);

  // Final phase of fetching: binary data is collected and ready to be
  // interpreted or error is encountered.
  virtual void OnResponse(std::unique_ptr<std::string> response_body) = 0;
  virtual void OnError(const ProtoFetcherStatus& status) = 0;

  // Returns payload when it's eligible for the request type.
  std::optional<std::string> GetRequestPayload() const;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  const std::string payload_;
  const raw_ref<const FetcherConfig> config_;
  const FetcherConfig::PathArgs args_;
  std::optional<version_info::Channel> channel_;
  std::optional<Metrics> metrics_;

  // Entrypoint of the fetch process, which starts with ApiAccessToken access
  // followed by a request made with SimpleURLLoader. Purposely made last field
  // should it depend on other members of this class.
  ApiAccessTokenFetcher fetcher_;

  // If an auth error was encountered when fetching the access token, it is
  // stored here (whether or not it was fatal).
  std::optional<GoogleServiceAuthError> access_token_auth_error_;
};

// Overlay over FetchProcess that interprets successful responses as given
// Response type parameter.
// Use instance of TypedFetchProcess to start request and write the result onto
// the receiving delegate. Every instance of Fetcher is disposable and should be
// used only once.
template <typename Response>
class TypedFetchProcess : public FetchProcess {
 public:
  // Called when fetch completes. The response contains value iff the status
  // doesn't signal error (see ProtoFetcherStatus::IsOK). In not-OK situations,
  // the response is empty.
  using Callback = base::OnceCallback<void(const ProtoFetcherStatus&,
                                           std::unique_ptr<Response>)>;
  TypedFetchProcess() = delete;
  TypedFetchProcess(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view payload,
      Callback callback,
      const FetcherConfig& fetcher_config,
      const FetcherConfig::PathArgs& args,
      std::optional<version_info::Channel> channel)
      : FetchProcess(identity_manager,
                     url_loader_factory,
                     payload,
                     fetcher_config,
                     args,
                     channel),
        callback_(std::move(callback)) {}

  ~TypedFetchProcess() override = default;

 private:
  void OnResponse(std::unique_ptr<std::string> response_body) override {
    CHECK(response_body) << "Use OnError when there is no response.";
    std::unique_ptr<Response> response = std::make_unique<Response>();
    if (!response->ParseFromString(*response_body)) {
      OnError(ProtoFetcherStatus::InvalidResponse());
      return;
    }

    OnSuccess(std::move(response));
  }

  void OnSuccess(std::unique_ptr<Response> response) {
    CHECK(response) << "ProtoFetcherStatus::Ok implies non-empty response "
                       "(which is always a valid message).";
    RecordMetrics(ProtoFetcherStatus::Ok());
    std::move(callback_).Run(ProtoFetcherStatus::Ok(), std::move(response));
  }

  void OnError(const ProtoFetcherStatus& status) override {
    RecordMetrics(status);
    std::move(callback_).Run(status, nullptr);
  }

  Callback callback_;
};

// Proto fetcher owns the fetch process(es). Depending on the requested
// configuration, there might be multiple processes within one fetch.
template <typename Response>
class ProtoFetcher final {
 public:
  using Callback = TypedFetchProcess<Response>::Callback;

  ProtoFetcher() = delete;
  ProtoFetcher(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view request,
      TypedFetchProcess<Response>::Callback callback,
      const FetcherConfig& fetcher_config,
      FetcherConfig::PathArgs args,
      std::optional<version_info::Channel> channel)
      : callback_(std::move(callback)),
        factory_(base::BindRepeating(&ProtoFetcher<Response>::Factory,
                                     base::Unretained(this),
                                     std::ref(identity_manager),
                                     url_loader_factory,
                                     request,
                                     fetcher_config,
                                     args,
                                     channel)),
        backoff_entry_(fetcher_config.BackoffEntry()),
        overall_metrics_(OverallMetrics::FromConfig(fetcher_config)) {
    Fetch();
  }

  // Not copyable.
  ProtoFetcher(const ProtoFetcher&) = delete;
  ProtoFetcher& operator=(const ProtoFetcher&) = delete;

 private:
  std::unique_ptr<TypedFetchProcess<Response>> Factory(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view request,
      const FetcherConfig& fetcher_config,
      FetcherConfig::PathArgs args,
      std::optional<version_info::Channel> channel) {
    return std::make_unique<TypedFetchProcess<Response>>(
        identity_manager, url_loader_factory, request,
        base::BindOnce(&ProtoFetcher<Response>::OnResponse,
                       base::Unretained(this)),
        fetcher_config, args, channel);
  }

  void Stop() {
    fetcher_.reset();
    timer_.Stop();
  }

  void Fetch() {
    retry_count_++;
    fetcher_ = factory_.Run();
  }

  bool HasRetrySupportEnabled() const {
    return static_cast<bool>(backoff_entry_);
  }

  bool ShouldRetry(const ProtoFetcherStatus& status) {
    return HasRetrySupportEnabled() && status.IsTransientError();
  }

  void OnResponse(const ProtoFetcherStatus& status,
                  std::unique_ptr<Response> response) {
    if (ShouldRetry(status)) {
      Stop();
      backoff_entry_->InformOfRequest(/*succeeded=*/false);
      timer_.Start(FROM_HERE, backoff_entry_->GetTimeUntilRelease(), this,
                   &ProtoFetcher<Response>::Fetch);
      return;
    }

    CHECK(callback_) << "Callback can be used only once.";

    if (HasRetrySupportEnabled()) {
      backoff_entry_->InformOfRequest(/*succeeded=*/true);
      if (IsOverallMetricsRecordingEnabled()) {
        overall_metrics_->RecordLatency();
        overall_metrics_->RecordStatus(status);
        overall_metrics_->RecordRetryCount(retry_count_);
      }
    }

    std::move(callback_).Run(status, std::move(response));
  }

  inline bool IsOverallMetricsRecordingEnabled() const {
    return overall_metrics_.has_value();
  }

  // Client callback.
  TypedFetchProcess<Response>::Callback callback_;
  base::RepeatingCallback<std::unique_ptr<TypedFetchProcess<Response>>(void)>
      factory_;
  std::unique_ptr<TypedFetchProcess<Response>> fetcher_;

  // Retry controls.
  base::OneShotTimer timer_;
  std::unique_ptr<net::BackoffEntry> backoff_entry_;
  int retry_count_{0};
  const std::optional<OverallMetrics> overall_metrics_;
};

// Constructs a launched fetcher. The fetcher will be either one shot or
// retryable, depending on the FetcherConfig::backoff_policy setting.
// `identity_manager` and `fetcher_config` must outlive this call.
//
// `args` are only relevant if `fetcher_config` uses template path (see
// supervised_user::FetcherConfig::service_path).
//
// `channel` must be specified if `fetcher_config` has
// `CredentialsRequirement::kBestEffort`.
template <typename Response>
std::unique_ptr<ProtoFetcher<Response>> CreateFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const google::protobuf::MessageLite& request,
    typename ProtoFetcher<Response>::Callback callback,
    const FetcherConfig& fetcher_config,
    const FetcherConfig::PathArgs& args = {},
    const std::optional<version_info::Channel> channel = std::nullopt) {
  CHECK((fetcher_config.access_token_config.credentials_requirement !=
         AccessTokenConfig::CredentialsRequirement::kBestEffort) ||
        channel)
      << "The Chrome channel must be specified for fetchers which can send "
         "requests without user credentials.";
  return std::make_unique<ProtoFetcher<Response>>(
      identity_manager, url_loader_factory, request.SerializeAsString(),
      std::move(callback), fetcher_config, args, channel);
}

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_H_
