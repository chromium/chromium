// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MANTA_MANTA_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_MANTA_MANTA_SERVICE_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace manta {
class MantaService;
}  // namespace manta

namespace ash {

// Provides the manta::MantaService associated with a user to ChromeOS callers
// without forcing them to depend on //chrome/browser/manta's
// MantaServiceFactory. The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// manta_service_provider_impl.h) and resolves the account to its BrowserContext
// before calling the factory.
class COMPONENT_EXPORT(MANTA_SERVICE_PROVIDER) MantaServiceProvider {
 public:
  MantaServiceProvider();
  MantaServiceProvider(const MantaServiceProvider&) = delete;
  MantaServiceProvider& operator=(const MantaServiceProvider&) = delete;
  virtual ~MantaServiceProvider();

  // Returns the process-wide singleton.
  static MantaServiceProvider& Get();

  // Returns the MantaService associated with `account_id`, or nullptr if the
  // embedder provides none (e.g. the service is disabled for the user). The
  // returned pointer is owned by the BrowserContext-keyed service
  // infrastructure; callers must not delete it.
  virtual manta::MantaService* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MANTA_MANTA_SERVICE_PROVIDER_H_
