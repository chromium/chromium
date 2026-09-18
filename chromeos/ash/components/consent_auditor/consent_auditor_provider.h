// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace ash {

// Provides the consent_auditor::ConsentAuditor associated with a user to
// ChromeOS callers without forcing them to depend on
// //chrome/browser/consent_auditor's Profile-keyed factory. The concrete
// implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// consent_auditor_provider_impl.h).
class COMPONENT_EXPORT(CONSENT_AUDITOR_PROVIDER) ConsentAuditorProvider {
 public:
  ConsentAuditorProvider();
  ConsentAuditorProvider(const ConsentAuditorProvider&) = delete;
  ConsentAuditorProvider& operator=(const ConsentAuditorProvider&) = delete;
  virtual ~ConsentAuditorProvider();

  // Returns the process-wide singleton.
  static ConsentAuditorProvider& Get();

  // Returns the ConsentAuditor associated with `account_id`, or nullptr if none
  // is available. The returned pointer is owned by the BrowserContext-keyed
  // service infrastructure; callers must not delete it.
  virtual consent_auditor::ConsentAuditor* Find(
      const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_PROVIDER_H_
