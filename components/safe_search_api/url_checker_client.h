// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_CLIENT_H_
#define COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_CLIENT_H_

#include "base/callback_forward.h"
#include "url/gurl.h"

namespace safe_search_api {

// The client representation of a URL classification by the service for the user
// in the request context.
enum class ClientClassification { kAllowed, kRestricted, kUnknown };

// Interface to make the server request and check an URL.
class URLCheckerClient {
 public:
  // Used to report whether |url| should be blocked. Called from CheckURL.
  using ClientCheckCallback =
      base::OnceCallback<void(const GURL&,
                              ClientClassification classification)>;

  virtual ~URLCheckerClient() = default;

  // Checks whether an |url| is restricted for the user in the request context.
  //
  // On success, the |callback| function is called with |url| as the first
  // parameter, the result as second.
  //
  // Refer to the implementation class for documentation about error handling.
  virtual void CheckURL(const GURL& url, ClientCheckCallback callback) = 0;
};

}  // namespace safe_search_api

#endif  // COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_CLIENT_H_
