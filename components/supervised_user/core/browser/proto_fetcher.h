// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/types/strong_alias.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto/permissions_common.pb.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
class ProtoFetcherStatus {
 public:
  using HttpStatusOrNetErrorType =
      base::StrongAlias<class HttpStatusOrNetErrorTag, int>;

  enum State {
    OK = 0,
    GOOGLE_SERVICE_AUTH_ERROR = 1,  // Error occurred during the access token
                                    // fetching phase. See
                                    // GetGoogleServiceAuthError for details.
    HTTP_STATUS_OR_NET_ERROR =
        2,  // The request was performed, but network or http returned errors.
            // This is default chromium approach to combine those two domains.
    INVALID_RESPONSE = 3,  // The request was performed without error, but http
                           // response could not be processed or was unexpected.
    DATA_ERROR = 4,  // The request was parsed, but did not contain all required
                     // data. Not signalled by this fetcher itself, but might be
                     // used by consumers to indicate data problem.
    kMaxValue = DATA_ERROR,  // keep last, required for metrics.
  };

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

  // Translate the status to metric enum label as defined in
  // tools/metrics/histograms/enums.xml.
  std::string ToMetricEnumLabel() const;

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

// Use instance of ProtoFetcher to start request and write the result onto the
// receiving delegate. Every instance of Fetcher is disposable and should be
// used only once.
template <typename Response>
class ProtoFetcher {
 public:
  using Callback =
      base::OnceCallback<void(ProtoFetcherStatus, std::unique_ptr<Response>)>;
  virtual ~ProtoFetcher() = default;
};

// Use instance of DeferredProtoFetcher to create fetch process which is
// unstarted yet.
template <typename Response>
class DeferredProtoFetcher : public ProtoFetcher<Response> {
 public:
  virtual void Start(typename ProtoFetcher<Response>::Callback callback) = 0;
};

// Creates a disposable instance of an access token consumer that will fetch
// list of family members.
std::unique_ptr<ProtoFetcher<kids_chrome_management::ListFamilyMembersResponse>>
FetchListFamilyMembers(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ProtoFetcher<kids_chrome_management::ListFamilyMembersResponse>::Callback
        callback,
    const FetcherConfig& config = kListFamilyMembersConfig);

// Creates a disposable instance of an access token consumer that will classify
// the URL for supervised user.
std::unique_ptr<ProtoFetcher<kids_chrome_management::ClassifyUrlResponse>>
ClassifyURL(signin::IdentityManager& identity_manager,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
            const kids_chrome_management::ClassifyUrlRequest& request,
            ProtoFetcher<kids_chrome_management::ClassifyUrlResponse>::Callback
                callback,
            const FetcherConfig& config = kClassifyUrlConfig);

// Creates a disposable instance of an access token consumer that will create
// a new permission request for a given url.
// The fetcher does not need to use the `CreatePermissionRequestRequest`
// message. The `request` input corresponds to a `PermissionRequest` message,
// which is mapped to the body of the `CreatePermissionRequestRequest`
// message by the http to gRPC mapping on the server side.
// See go/rpc-create-permission-request.
std::unique_ptr<DeferredProtoFetcher<
    kids_chrome_management::CreatePermissionRequestResponse>>
CreatePermissionRequestFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kids_chrome_management::PermissionRequest& request,
    const FetcherConfig& config = kCreatePermissionRequestConfig);

}  // namespace supervised_user
#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PROTO_FETCHER_H_
