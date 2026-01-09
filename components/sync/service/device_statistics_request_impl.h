// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_REQUEST_IMPL_H_
#define COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_REQUEST_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/device_statistics_request.h"
#include "url/gurl.h"

class GoogleServiceAuthError;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace sync_pb {
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

class DeviceStatisticsRequestImpl : public DeviceStatisticsRequest {
 public:
  // Constructs a request for the given account (which does not have to be the
  // primary account). `identity_manager` and `url_loader_factory` must not be
  // null.
  DeviceStatisticsRequestImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view user_agent,
      const CoreAccountInfo& account,
      const GURL& url);
  DeviceStatisticsRequestImpl(const DeviceStatisticsRequestImpl&) = delete;
  DeviceStatisticsRequestImpl(DeviceStatisticsRequestImpl&&) = delete;
  ~DeviceStatisticsRequestImpl() override;

  void Start(base::OnceClosure callback) override;

  State GetState() const override;
  const std::vector<sync_pb::SyncEntity>& GetResults() const override;

 private:
  void AccessTokenFetchComplete(GoogleServiceAuthError error,
                                signin::AccessTokenInfo access_token_info);
  void SimpleLoaderComplete(signin::AccessTokenInfo access_token_info,
                            std::optional<std::string> response_body);

  void UpdateStateAndNotify(State state);

  const CoreAccountInfo account_;

  const raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const GURL url_;

  const std::string user_agent_;

  State state_ = State::kNotStarted;

  base::OnceClosure callback_;

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  bool has_retried_authorization_ = false;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  std::vector<sync_pb::SyncEntity> results_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_REQUEST_IMPL_H_
