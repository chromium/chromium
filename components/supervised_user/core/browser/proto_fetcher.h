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
#include "base/types/strong_alias.h"
#include "base/version_info/channel.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/api_access_token_fetcher.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto/permissions_common.pb.h"
#include "components/supervised_user/core/browser/proto/test.pb.h"
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

// Holds the status of the fetch. The callback's response will be set iff the
// status is ok.
class ProtoFetcherStatus {
 public:
  using HttpStatusOrNetErrorType =
      base::StrongAlias<class HttpStatusOrNetErrorTag, int>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(State)
  enum State {
    OK = 0,
    GOOGLE_SERVICE_AUTH_ERROR = 1,
    HTTP_STATUS_OR_NET_ERROR = 2,
    INVALID_RESPONSE = 3,
    DATA_ERROR = 4,  //  Not signalled by this fetcher itself, but might be used
                     //  by consumers to indicate data problem.
    kMaxValue = DATA_ERROR,  // keep last, required for metrics.
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:SupervisedUserProtoFetcherStatus)

  // Status might be used in base::expected context as possible error, since it
  // contains two error-enabled attributes which are copyable / assignable.
  ProtoFetcherStatus(const ProtoFetcherStatus&);
  ProtoFetcherStatus& operator=(const ProtoFetcherStatus&);

  ~ProtoFetcherStatus();
  ProtoFetcherStatus() = delete;

  // Convenience creators instead of exposing ProtoFetcherStatus(State state).
  static ProtoFetcherStatus Ok();
  static ProtoFetcherStatus GoogleServiceAuthError(
      GoogleServiceAuthError
          error);  // The copy follows the interface of
                   // https://source.chromium.org/chromium/chromium/src/+/main:components/signin/public/identity_manager/primary_account_access_token_fetcher.h;l=241;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd
  static ProtoFetcherStatus HttpStatusOrNetError(
      int value = 0);  // Either net::Error (negative numbers, 0 denotes
                       // success) or HTTP status.
  static ProtoFetcherStatus InvalidResponse();
  static ProtoFetcherStatus DataError();

  // ProtoFetcherStatus::IsOk iff google_service_auth_error_.state() ==
  // NONE and state_ == NONE
  bool IsOk() const;
  // Indicates whether the status is not ok, but is worth retrying because it
  // might go away.
  bool IsTransientError() const;
  // Indicates whether the status is not ok and there is no point in retrying.
  bool IsPersistentError() const;

  // Returns a message describing the status.
  std::string ToString() const;

  State state() const;
  HttpStatusOrNetErrorType http_status_or_net_error() const;
  const class GoogleServiceAuthError& google_service_auth_error() const;

 private:
  // Disallows impossible states.
  explicit ProtoFetcherStatus(State state);
  explicit ProtoFetcherStatus(
      HttpStatusOrNetErrorType http_status_or_net_error);
  explicit ProtoFetcherStatus(
      class GoogleServiceAuthError
          google_service_auth_error);  // Implies State ==
                                       // GOOGLE_SERVICE_AUTH_ERROR
  ProtoFetcherStatus(State state,
                     class GoogleServiceAuthError google_service_auth_error);

  State state_;
  HttpStatusOrNetErrorType http_status_or_net_error_{
      0};  // Meaningful iff state_ == HTTP_STATUS_OR_NET_ERROR
  class GoogleServiceAuthError google_service_auth_error_;
};

// A stopwatch with two functions:
// * measure total elapsed time,
// * measure lap time (with automatic resetting after each lap).
// The stopwatch is created started.
class Stopwatch {
 public:
  // Time since start of last lap. Resets the lap timer.
  base::TimeDelta Lap();
  // Time since start of last lap.
  base::TimeDelta Elapsed() const;

 private:
  base::ElapsedTimer elapsed_timer_;
  base::ElapsedTimer lap_timer_;
};

// Encapsulates metric functionalities.
class Metrics {
 public:
  enum class MetricType {
    kStatus,
    kLatency,
    kHttpStatusOrNetError,
    kRetryCount,
    kAccessTokenLatency,
    kApiLatency,
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
  Stopwatch stopwatch_;
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
class AbstractProtoFetcher {
 public:
  AbstractProtoFetcher() = delete;
  AbstractProtoFetcher(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view payload,
      const FetcherConfig& fetcher_config,
      const FetcherConfig::PathArgs& args = {},
      std::optional<version_info::Channel> channel = std::nullopt);

  // Not copyable.
  AbstractProtoFetcher(const AbstractProtoFetcher&) = delete;
  AbstractProtoFetcher& operator=(const AbstractProtoFetcher&) = delete;

  virtual ~AbstractProtoFetcher();
  virtual bool IsMetricsRecordingEnabled() const;

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

 protected:
  // Final phase of fetching: binary data is collected and ready to be
  // interpreted or error is encountered.
  virtual void OnResponse(std::unique_ptr<std::string> response_body) = 0;
  virtual void OnError(const ProtoFetcherStatus& status) = 0;

 private:
  // Returns payload when it's eligible for the request type.
  std::optional<std::string> GetRequestPayload() const;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  const std::string payload_;
  const FetcherConfig config_;
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

// Overlay over ProtoFetcher that interprets successful responses as given
// Response type parameter.
// Use instance of TypedProtoFetcher to start request and write the result onto
// the receiving delegate. Every instance of Fetcher is disposable and should be
// used only once.
template <typename Response>
class TypedProtoFetcher : public AbstractProtoFetcher {
 public:
  // Called when fetch completes. The response contains value iff the status
  // doesn't signal error (see ProtoFetcherStatus::IsOK). In not-OK situations,
  // the response is empty.
  using Callback = base::OnceCallback<void(const ProtoFetcherStatus&,
                                           std::unique_ptr<Response>)>;
  TypedProtoFetcher() = delete;
  TypedProtoFetcher(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view payload,
      const FetcherConfig& fetcher_config,
      const FetcherConfig::PathArgs& args,
      std::optional<version_info::Channel> channel,
      Callback callback)
      : AbstractProtoFetcher(identity_manager,
                             url_loader_factory,
                             payload,
                             fetcher_config,
                             args,
                             channel),
        callback_(std::move(callback)) {}

  virtual ~TypedProtoFetcher() = default;

 protected:
  void OnError(const ProtoFetcherStatus& status) override {
    RecordMetrics(status);
    std::move(callback_).Run(status, nullptr);
  }

  void OnResponse(std::unique_ptr<std::string> response_body) override {
    CHECK(response_body) << "Use OnError when there is no response.";
    std::unique_ptr<Response> response = std::make_unique<Response>();
    if (!response->ParseFromString(*response_body)) {
      OnError(ProtoFetcherStatus::InvalidResponse());
      return;
    }

    OnSuccess(std::move(response));
  }

 private:
  void OnSuccess(std::unique_ptr<Response> response) {
    CHECK(response) << "ProtoFetcherStatus::Ok implies non-empty response "
                       "(which is always a valid message).";
    RecordMetrics(ProtoFetcherStatus::Ok());
    std::move(callback_).Run(ProtoFetcherStatus::Ok(), std::move(response));
  }

  Callback callback_;
};

// Overlay over ProtoFetcher that only cares about status of the response.
// Every instance of Fetcher is disposable and should be
// used only once.
class StatusFetcher : public AbstractProtoFetcher {
 public:
  using Callback = base::OnceCallback<void(const ProtoFetcherStatus&)>;

  StatusFetcher() = delete;
  StatusFetcher(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view payload,
      const FetcherConfig& fetcher_config,
      const FetcherConfig::PathArgs& args,
      std::optional<version_info::Channel> channel,
      Callback callback);
  ~StatusFetcher() override;

 protected:
  void OnError(const ProtoFetcherStatus& status) override;
  void OnResponse(std::unique_ptr<std::string> response_body) override;

 private:
  void OnStatus(const ProtoFetcherStatus& status);
  Callback callback_;
};

// Use instance of ProtoFetcher to create fetch process which is
// unstarted yet.
template <typename Response>
class ProtoFetcher {
 public:
  using Callback = typename TypedProtoFetcher<Response>::Callback;

  ProtoFetcher() = delete;
  ProtoFetcher(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const google::protobuf::MessageLite& request,
      const FetcherConfig& fetcher_config,
      const FetcherConfig::PathArgs& args = {},
      std::optional<version_info::Channel> channel = std::nullopt)
      : identity_manager_(identity_manager),
        url_loader_factory_(url_loader_factory),
        payload_(request.SerializeAsString()),
        config_(fetcher_config),
        args_(args),
        channel_(channel) {}
  virtual ~ProtoFetcher() = default;

  // Kicks off the fetching process. The fetcher must not be started before
  // calling this method.
  virtual void Start(Callback callback) {
    CHECK(!fetcher_) << "Only stopped fetcher can be started.";
    fetcher_ = std::make_unique<TypedProtoFetcher<Response>>(
        identity_manager_.get(), url_loader_factory_, payload_, config_, args_,
        channel_, std::move(callback));
  }

  // Terminates the fetching process. The fetcher must be still operating while
  // calling this method.
  virtual void Stop() {
    CHECK(fetcher_) << "Only started fetcher can be stopped.";
    fetcher_.reset();
  }

  virtual bool IsMetricsRecordingEnabled() const {
    return fetcher_->IsMetricsRecordingEnabled();
  }

 private:
  const raw_ref<signin::IdentityManager, LeakedDanglingUntriaged>
      identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::string payload_;
  const FetcherConfig config_;
  const FetcherConfig::PathArgs args_;
  std::optional<version_info::Channel> channel_;
  std::unique_ptr<TypedProtoFetcher<Response>> fetcher_;
};

// A subtype of DeferredProtoFetcher that will take retrying strategy as
// specified in FetcherConfig::backoff_policy.
//
// The retries are only performed on transient errors (see ::ShouldRetry).
template <typename Response>
class RetryingFetcherImpl final : public ProtoFetcher<Response> {
 public:
  RetryingFetcherImpl() = delete;

  RetryingFetcherImpl(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const google::protobuf::MessageLite& request,
      const FetcherConfig& fetcher_config,
      const FetcherConfig::PathArgs& args,
      std::optional<version_info::Channel> channel,
      const net::BackoffEntry::Policy& backoff_policy)
      : ProtoFetcher<Response>(identity_manager,
                               url_loader_factory,
                               request,
                               fetcher_config,
                               args,
                               channel),
        backoff_entry_(&backoff_policy),
        metrics_(OverallMetrics::FromConfig(fetcher_config)) {}

  // Not copyable.
  RetryingFetcherImpl(const RetryingFetcherImpl&) = delete;
  RetryingFetcherImpl& operator=(const RetryingFetcherImpl&) = delete;

  void Start(ProtoFetcher<Response>::Callback callback) override {
    callback_ = std::move(callback);
    Retry();
  }
  void Stop() override {
    ProtoFetcher<Response>::Stop();
    timer_.Stop();
  }

  bool IsMetricsRecordingEnabled() const override {
    return metrics_.has_value();
  }

 private:
  void Retry() {
    retry_count_++;
    ProtoFetcher<Response>::Start(
        base::BindOnce(&RetryingFetcherImpl<Response>::OnRetriedResponse,
                       base::Unretained(this)));
  }

  bool ShouldRetry(const ProtoFetcherStatus& status) {
    return status.IsTransientError();
  }

  void OnRetriedResponse(const ProtoFetcherStatus& status,
                         std::unique_ptr<Response> response) {
    if (ShouldRetry(status)) {
      Stop();
      backoff_entry_.InformOfRequest(/*succeeded=*/false);
      timer_.Start(FROM_HERE, backoff_entry_.GetTimeUntilRelease(), this,
                   &RetryingFetcherImpl<Response>::Retry);
      return;
    }

    CHECK(callback_) << "Callback can be used only once.";
    backoff_entry_.InformOfRequest(/*succeeded=*/true);
    if (IsMetricsRecordingEnabled()) {
      metrics_->RecordLatency();
      metrics_->RecordStatus(status);
      metrics_->RecordRetryCount(retry_count_);
    }
    std::move(callback_).Run(status, std::move(response));
  }

  // Client callback.
  TypedProtoFetcher<Response>::Callback callback_;

  // Retry controls.
  base::OneShotTimer timer_;
  net::BackoffEntry backoff_entry_;
  int retry_count_{0};

  const std::optional<OverallMetrics> metrics_;
};

// Component for managing multiple fetches at once.
//
// After each fetch, the reference kept in internal map is cleared. This will
// also happen when this manager is destroyed. In the latter case, callbacks
// won't be executed (the pending requests will be canceled).
template <typename Request, typename Response>
class ParallelFetchManager {
 private:
  // Deferred fetcher is required because it should be started after it is
  // stored internally.
  using Fetcher = ProtoFetcher<Response>;
  using KeyType = base::IDMap<std::unique_ptr<Fetcher>>::KeyType;

 public:
  // Provides fresh instances of a deferred fetcher for each fetch.
  using FetcherFactory =
      base::RepeatingCallback<std::unique_ptr<Fetcher>(const Request&)>;

  ParallelFetchManager() = delete;
  explicit ParallelFetchManager(FetcherFactory fetcher_factory)
      : fetcher_factory_(fetcher_factory) {}
  ParallelFetchManager(const ParallelFetchManager&) = delete;
  ParallelFetchManager& operator=(const ParallelFetchManager&) = delete;
  ~ParallelFetchManager() = default;

  // Starts the fetch. Underlying fetcher is stored internally, and will be
  // cleaned up after finish or when this manager is destroyed.
  void Fetch(const Request& request, Fetcher::Callback callback) {
    CHECK(callback) << "Use base::DoNothing() instead of empty callback.";
    KeyType key = requests_in_flight_.Add(MakeFetcher(request));
    requests_in_flight_.Lookup(key)->Start(
        std::move(callback).Then(base::BindOnce(
            &ParallelFetchManager::Remove, weak_factory_.GetWeakPtr(), key)));
  }

 private:
  // Remove fetcher under key from requests_in_flight_.
  void Remove(KeyType key) { requests_in_flight_.Remove(key); }

  std::unique_ptr<Fetcher> MakeFetcher(const Request& request) const {
    return fetcher_factory_.Run(request);
  }

  base::IDMap<std::unique_ptr<Fetcher>, KeyType> requests_in_flight_;
  FetcherFactory fetcher_factory_;
  base::WeakPtrFactory<ParallelFetchManager<Request, Response>> weak_factory_{
      this};
};

namespace {

using ClassifyUrlFetcher = ProtoFetcher<kidsmanagement::ClassifyUrlResponse>;
using ListFamilyMembersFetcher =
    ProtoFetcher<kidsmanagement::ListMembersResponse>;
using PermissionRequestFetcher =
    ProtoFetcher<kidsmanagement::CreatePermissionRequestResponse>;
}  // namespace

// Fetches list family members. The returned fetcher is already started.
std::unique_ptr<ProtoFetcher<kidsmanagement::ListMembersResponse>>
FetchListFamilyMembers(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TypedProtoFetcher<kidsmanagement::ListMembersResponse>::Callback callback,
    const FetcherConfig& config = kListFamilyMembersConfig);

// Creates a disposable instance of an access token consumer that will classify
// the URL for supervised user.
std::unique_ptr<ProtoFetcher<kidsmanagement::ClassifyUrlResponse>>
CreateClassifyURLFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::ClassifyUrlRequest& request,
    const FetcherConfig& config = kClassifyUrlConfig,
    version_info::Channel channel = version_info::Channel::UNKNOWN);

// Creates a disposable instance of an access token consumer that will create
// a new permission request for a given url.
// The fetcher does not need to use the `CreatePermissionRequestRequest`
// message. The `request` input corresponds to a `PermissionRequest` message,
// which is mapped to the body of the `CreatePermissionRequestRequest`
// message by the http to gRPC mapping on the server side.
// See go/rpc-create-permission-request.
std::unique_ptr<ProtoFetcher<kidsmanagement::CreatePermissionRequestResponse>>
CreatePermissionRequestFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::PermissionRequest& request,
    const FetcherConfig& config = kCreatePermissionRequestConfig);

std::unique_ptr<ProtoFetcher<Response>> CreateTestFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const Request& request,
    const FetcherConfig& fetcher_config);

// Constructs a fetcher that needs to be launched with ::Start(). The fetcher
// will be either one shot or retryable, depending on the
// FetcherConfig::backoff_policy setting.
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
    const FetcherConfig& fetcher_config,
    const FetcherConfig::PathArgs& args = {},
    const std::optional<version_info::Channel> channel = std::nullopt) {
  CHECK((fetcher_config.access_token_config.credentials_requirement !=
         AccessTokenConfig::CredentialsRequirement::kBestEffort) ||
        channel)
      << "The Chrome channel must be specified for fetchers which can send "
         "requests without user credentials.";

  if (fetcher_config.backoff_policy.has_value()) {
    return std::make_unique<RetryingFetcherImpl<Response>>(
        identity_manager, url_loader_factory, request, fetcher_config, args,
        channel, *fetcher_config.backoff_policy);
  } else {
    return std::make_unique<ProtoFetcher<Response>>(
        identity_manager, url_loader_factory, request, fetcher_config, args,
        channel);
  }
}

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_H_
