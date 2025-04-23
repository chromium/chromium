// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_USER_SECURITY_SIGNALS_SERVICE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_USER_SECURITY_SIGNALS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/wall_clock_timer.h"
#include "components/enterprise/browser/reporting/security_signals_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace enterprise_reporting {

// Service in charge of the scheduling, and triggering, generation and upload of
// user security signals reports.
class UserSecuritySignalsService : public SecuritySignalsService {
 public:
  class Delegate {
   public:
    // Invoked when `trigger` happens, and will only be invoked if the current
    // policy values dictate that this trigger is meaningful.
    virtual void OnReportEventTriggered(SecurityReportTrigger trigger) = 0;
  };

  UserSecuritySignalsService(PrefService* profile_prefs, Delegate* delegate);
  ~UserSecuritySignalsService() override;

  UserSecuritySignalsService(const UserSecuritySignalsService&) = delete;
  UserSecuritySignalsService& operator=(const UserSecuritySignalsService&) =
      delete;

  // Returns the time delta representing the cadence at which security reports
  // will be triggered.
  static base::TimeDelta GetSecurityUploadCadence();

  // SecuritySignalsService:
  void Start() override;
  bool IsSecuritySignalsReportingEnabled() override;
  bool ShouldUseCookies() override;
  void OnReportUploaded() override;

 private:
  // Called when the state value of the policy changes. This will affect whether
  // the timed reporting loop is running or not.
  void OnStatePolicyValueChanged();

  // Will notify observers that a report needs to be generated and uploaded due
  // to `trigger`.
  void TriggerReport(SecurityReportTrigger trigger);

  const raw_ptr<PrefService> profile_prefs_;
  const raw_ptr<Delegate> delegate_;
  PrefChangeRegistrar pref_change_registrar_;
  base::WallClockTimer timer_;
  bool initialized_{false};

  base::WeakPtrFactory<UserSecuritySignalsService> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_USER_SECURITY_SIGNALS_SERVICE_H_
