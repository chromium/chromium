// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MANTA_FAKE_MANTA_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_MANTA_FAKE_MANTA_SERVICE_PROVIDER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/manta/manta_service_provider.h"
#include "components/account_id/account_id.h"

namespace manta {
class MantaService;
}  // namespace manta

namespace ash {

// Test MantaServiceProvider that maps account ids to MantaService instances.
// Installs itself as the process-wide provider on construction. Find() returns
// a service only for accounts explicitly registered via
// SetMantaServiceForAccount (and nullptr otherwise), so a service is never
// handed back for a user that a test has not set up as signed-in.
class FakeMantaServiceProvider : public MantaServiceProvider {
 public:
  FakeMantaServiceProvider();
  ~FakeMantaServiceProvider() override;

  // Registers `manta_service` for `account_id`. Passing nullptr clears the
  // association.
  void SetMantaServiceForAccount(const AccountId& account_id,
                                 manta::MantaService* manta_service);

  // MantaServiceProvider:
  manta::MantaService* Find(const AccountId& account_id) override;

 private:
  std::map<AccountId, raw_ptr<manta::MantaService>> manta_services_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MANTA_FAKE_MANTA_SERVICE_PROVIDER_H_
