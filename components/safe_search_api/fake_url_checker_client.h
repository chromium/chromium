// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_SEARCH_API_FAKE_URL_CHECKER_CLIENT_H_
#define COMPONENTS_SAFE_SEARCH_API_FAKE_URL_CHECKER_CLIENT_H_

#include "base/callback.h"
#include "components/safe_search_api/url_checker_client.h"

namespace safe_search_api {

// Helper class with fake URLCheckerClient for use with URLChecker. This
// lets tests control the response the URLChecker will receive from the
// URLCheckerClient. Used to test URLChecker.
class FakeURLCheckerClient : public URLCheckerClient {
 public:
  FakeURLCheckerClient();
  ~FakeURLCheckerClient() override;

  // Fake override that simply holds references of |url| and |callback|.
  //
  // See RunCallback() method documentation below on how to run the callback.
  void CheckURL(const GURL& url, ClientCheckCallback callback) override;

  // Run the callback function input by the last call of CheckURL() with the
  // result input with the last call of SetResult().
  void RunCallback(ClientClassification classification);

 private:
  ClientCheckCallback callback_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLCheckerClient);
};

}  // namespace safe_search_api

#endif  // COMPONENTS_SAFE_SEARCH_API_FAKE_URL_CHECKER_CLIENT_H_
