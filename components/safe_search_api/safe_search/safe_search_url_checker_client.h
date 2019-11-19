// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_SEARCH_API_SAFE_SEARCH_SAFE_SEARCH_URL_CHECKER_CLIENT_H_
#define COMPONENTS_SAFE_SEARCH_API_SAFE_SEARCH_SAFE_SEARCH_URL_CHECKER_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/safe_search_api/url_checker_client.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_search_api {

// This class uses the SafeSearch API to check the SafeSearch classification
// of the content on a given URL and returns the result asynchronously
// via a callback.
class SafeSearchURLCheckerClient : public URLCheckerClient {
 public:
  // |country| should be a two-letter country code (ISO 3166-1 alpha-2), e.g.,
  // "us". Optional
  SafeSearchURLCheckerClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const std::string& country = std::string(),
      const std::string& api_key = google_apis::GetAPIKey());

  ~SafeSearchURLCheckerClient() override;

  // Checks whether an |url| is restricted according to SafeSearch.
  //
  // On failure, the |callback| function is called with |url| as the first
  // parameter, and UNKNOWN as the second.
  void CheckURL(const GURL& url, ClientCheckCallback callback) override;

 private:
  struct Check;

  using CheckList = std::list<std::unique_ptr<Check>>;

  void OnSimpleLoaderComplete(CheckList::iterator it,
                              std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  const std::string country_;
  const std::string api_key_;

  CheckList checks_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(SafeSearchURLCheckerClient);
};

}  // namespace safe_search_api

#endif  // COMPONENTS_SAFE_SEARCH_API_SAFE_SEARCH_SAFE_SEARCH_URL_CHECKER_CLIENT_H_
