// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_URL_VALIDITY_CHECKER_H_
#define COMPONENTS_SEARCH_URL_VALIDITY_CHECKER_H_

#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

// A standalone service that validates if the provided URL is able to resolve to
// a valid page.
class UrlValidityChecker {
 public:
  // The callback invoked when the request completes. Returns true if the
  // response was |valid| and the request |duration|.
  using UrlValidityCheckerCallback =
      base::OnceCallback<void(bool valid, base::TimeDelta duration)>;

  virtual ~UrlValidityChecker() = default;

  // Creates a HEAD request to check if |url| resolves to an existing page.
  // Returns true if the URL resolves and the request duration. Redirects (3xx)
  // and 2xx response codes are considered as resolving.
  virtual void DoesUrlResolve(
      const GURL& url,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      UrlValidityCheckerCallback callback) = 0;
};

#endif  // COMPONENTS_SEARCH_URL_VALIDITY_CHECKER_H_
