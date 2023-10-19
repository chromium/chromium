// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

class GURL;

// TODO(crbug.com/988428): Change comments to use KidsChromeManagement instead
// of KidsManagement when migration is complete.

// This class uses the KidsManagement ClassifyUrl to check the classification
// of the content on a given URL and returns the result asynchronously
// via a callback.
class KidsManagementURLCheckerClient
    : public safe_search_api::URLCheckerClient {
 public:
  // |country| should be a two-letter country code (ISO 3166-1 alpha-2), e.g.,
  // "us".
  KidsManagementURLCheckerClient(
      KidsChromeManagementClient* kids_chrome_management_client,
      const std::string& country);

  KidsManagementURLCheckerClient(const KidsManagementURLCheckerClient&) =
      delete;
  KidsManagementURLCheckerClient& operator=(
      const KidsManagementURLCheckerClient&) = delete;

  ~KidsManagementURLCheckerClient() override;

  // Checks whether an |url| is restricted according to KidsManagement
  // ClassifyUrl RPC.
  //
  // On failure, the |callback| function is called with |url| as the first
  // parameter, and UNKNOWN as the second.
  void CheckURL(const GURL& url, ClientCheckCallback callback) override;

 private:
  void LegacyConvertResponseCallback(
      const GURL& url,
      ClientCheckCallback client_callback,
      std::unique_ptr<google::protobuf::MessageLite> response_proto,
      KidsChromeManagementClient::ErrorCode error_code);

  raw_ptr<KidsChromeManagementClient> kids_chrome_management_client_;
  safe_search_api::SafeSearchURLCheckerClient safe_search_client_;
  const std::string country_;

  supervised_user::ParallelFetchManager<
      kids_chrome_management::ClassifyUrlRequest,
      kids_chrome_management::ClassifyUrlResponse>
      fetch_manager_;

  base::WeakPtrFactory<KidsManagementURLCheckerClient> weak_factory_{this};
};

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_MANAGEMENT_URL_CHECKER_CLIENT_H_
