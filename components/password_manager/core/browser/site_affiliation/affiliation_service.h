// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_H_

#include <vector>

#include "base/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace password_manager {

class AffiliationService : public KeyedService {
 public:
  // Prefetches change password URLs for sites requested. Receives a callback to
  // run when the prefetch finishes.
  virtual void PrefetchChangePasswordURLs(const std::vector<GURL>& urls,
                                          base::OnceClosure callback) = 0;

  // Clears the result of URLs fetch.
  virtual void Clear() = 0;

  // Returns a URL with change password form for a site requested.
  virtual GURL GetChangePasswordURL(const GURL& url) const = 0;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_H_
