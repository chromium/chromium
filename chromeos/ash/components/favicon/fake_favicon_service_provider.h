// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FAVICON_FAKE_FAVICON_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_FAVICON_FAKE_FAVICON_SERVICE_PROVIDER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/favicon/favicon_service_provider.h"
#include "components/account_id/account_id.h"

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace ash {

// Test FaviconServiceProvider that maps account ids to FaviconService
// instances. Installs itself as the process-wide provider on construction.
// Find() returns a service only for accounts explicitly registered via
// SetFaviconServiceForAccount (and nullptr otherwise), so a service is never
// handed back for a user that a test has not set up as signed-in.
class FakeFaviconServiceProvider : public FaviconServiceProvider {
 public:
  FakeFaviconServiceProvider();
  ~FakeFaviconServiceProvider() override;

  // Registers `favicon_service` for `account_id`. Passing nullptr clears the
  // association.
  void SetFaviconServiceForAccount(const AccountId& account_id,
                                   favicon::FaviconService* favicon_service);

  // FaviconServiceProvider:
  favicon::FaviconService* Find(const AccountId& account_id) override;

 private:
  std::map<AccountId, raw_ptr<favicon::FaviconService>> favicon_services_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_FAVICON_FAKE_FAVICON_SERVICE_PROVIDER_H_
