// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FAVICON_FAVICON_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_FAVICON_FAVICON_SERVICE_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace ash {

// Provides the favicon::FaviconService associated with a user to ChromeOS
// callers without forcing them to depend on //chrome/browser/favicon's
// Profile-keyed factory. The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// favicon_service_provider_impl.h).
class COMPONENT_EXPORT(FAVICON_SERVICE_PROVIDER) FaviconServiceProvider {
 public:
  FaviconServiceProvider();
  FaviconServiceProvider(const FaviconServiceProvider&) = delete;
  FaviconServiceProvider& operator=(const FaviconServiceProvider&) = delete;
  virtual ~FaviconServiceProvider();

  // Returns the process-wide singleton.
  static FaviconServiceProvider& Get();

  // Returns the FaviconService associated with `account_id` (with
  // EXPLICIT_ACCESS), or nullptr if none is available. The returned pointer is
  // owned by the BrowserContext-keyed service infrastructure; callers must not
  // delete it.
  virtual favicon::FaviconService* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_FAVICON_FAVICON_SERVICE_PROVIDER_H_
