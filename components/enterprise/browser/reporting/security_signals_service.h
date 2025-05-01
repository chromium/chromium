// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SECURITY_SIGNALS_SERVICE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SECURITY_SIGNALS_SERVICE_H_

namespace enterprise_reporting {

// Set of triggers which the service uses as hook to trigger a security report.
// This enum should be kept in sync with the `SecurityReportTrigger` enum in
// tools/metrics/histograms/metadata/enterprise/enums.xml.
enum class SecurityReportTrigger {
  kTimer = 0,
  kCookieChange = 1,
  kMaxValue = kCookieChange
};

// Service in charge of the scheduling, and triggering, generation and upload of
// user security signals reports.
class SecuritySignalsService {
 public:
  virtual ~SecuritySignalsService() = default;

  // Hook used by the service to start monitoring its various triggers and
  // raising reporting events.
  virtual void Start() = 0;

  // Returns true if the user security signals reporting policy is enabled.
  virtual bool IsSecuritySignalsReportingEnabled() = 0;

  // Returns true if cookies should be used as authentication mechanism with the
  // device management server. This will automatically be false if the security
  // reporting policy is disabled.
  virtual bool ShouldUseCookies() = 0;

  // Use to tell the service that a report including security signals was
  // uploaded. This will postpone the next timed trigger.
  virtual void OnReportUploaded() = 0;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SECURITY_SIGNALS_SERVICE_H_
