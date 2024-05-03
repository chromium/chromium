// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_BACKEND_DELEGATE_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_BACKEND_DELEGATE_H_

#include <vector>

class GURL;

namespace favicon {

class FaviconBackendDelegate {
 public:
  FaviconBackendDelegate() = default;

  // Returns the redirects for `page_url`. This should always return a
  // vector with at least one element (`page_url`).
  virtual std::vector<GURL> GetCachedRecentRedirectsForPage(
      const GURL& page_url) = 0;
 protected:
  virtual ~FaviconBackendDelegate() = default;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_BACKEND_DELEGATE_H_
