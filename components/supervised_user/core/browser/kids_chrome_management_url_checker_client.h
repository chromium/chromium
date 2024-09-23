// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_CHROME_MANAGEMENT_URL_CHECKER_CLIENT_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_CHROME_MANAGEMENT_URL_CHECKER_CLIENT_H_

#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/parallel_fetch_manager.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class GURL;

namespace version_info {
enum class Channel;
}

namespace supervised_user {
// This class uses the KidsChromeManagement::ClassifyUrl to check the
// classification of the content on a given URL and returns the result
// asynchronously via a callback.
class KidsChromeManagementURLCheckerClient
    : public safe_search_api::URLCheckerClient {
 public:
  // `country` should be a two-letter country code (ISO 3166-1 alpha-2), e.g.,
  // "us".
  KidsChromeManagementURLCheckerClient(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string_view country,
      version_info::Channel channel);

  KidsChromeManagementURLCheckerClient(
      const KidsChromeManagementURLCheckerClient&) = delete;
  KidsChromeManagementURLCheckerClient& operator=(
      const KidsChromeManagementURLCheckerClient&) = delete;

  ~KidsChromeManagementURLCheckerClient() override;

  // Checks whether an `url` is restricted according to
  // KidsChromeManagement::ClassifyUrl RPC.
  //
  // On failure, the `callback` function is called with `url` as the first
  // parameter, and UNKNOWN as the second.
  void CheckURL(const GURL& url, ClientCheckCallback callback) override;

 private:
  const std::string country_;

  ParallelFetchManager<kidsmanagement::ClassifyUrlRequest,
                       kidsmanagement::ClassifyUrlResponse>
      fetch_manager_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_CHROME_MANAGEMENT_URL_CHECKER_CLIENT_H_
