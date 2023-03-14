// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_

#include <memory>

#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {

class GlanceablesTasksClientImpl;

// Browser context keyed service that owns implementations of interfaces from
// ash/ needed to communicate with different Google services as part of
// Glanceables project.
class GlanceablesKeyedService : public KeyedService {
 public:
  explicit GlanceablesKeyedService(Profile* profile);
  GlanceablesKeyedService(const GlanceablesKeyedService&) = delete;
  GlanceablesKeyedService& operator=(const GlanceablesKeyedService&) = delete;
  ~GlanceablesKeyedService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // Creates clients needed to communicate with different Google services.
  void CreateClients();

  // Notifies `ash/` about created clients for `account_id_`.
  void UpdateRegistrationInAsh() const;

  // Account id associated with the primary profile.
  const AccountId account_id_;

  // Instance of the `GlanceablesTasksClient` interface implementation.
  std::unique_ptr<GlanceablesTasksClientImpl> tasks_client_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_
