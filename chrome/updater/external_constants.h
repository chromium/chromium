// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_H_

#include <optional>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"

class GURL;

namespace base {
class TimeDelta;
}

namespace crx_file {
enum class VerifierFormat;
}

namespace updater {

// Several constants controlling the program's behavior can come from stateful
// external providers, such as dev-mode overrides or enterprise policies.
class ExternalConstants : public base::RefCountedThreadSafe<ExternalConstants> {
 public:
  explicit ExternalConstants(scoped_refptr<ExternalConstants> next_provider);
  ExternalConstants(const ExternalConstants&) = delete;
  ExternalConstants& operator=(const ExternalConstants&) = delete;

  // The URL to send update checks to.
  virtual std::vector<GURL> UpdateURL() const = 0;

  // The URL to send crash reports to.
  virtual GURL CrashUploadURL() const = 0;

  // The URL to fetch device management policies.
  virtual GURL DeviceManagementURL() const = 0;

  // The URL for the app logos.
  virtual GURL AppLogoURL() const = 0;

  // True if client update protocol signing of update checks is enabled.
  virtual bool UseCUP() const = 0;

  // Time to delay the start of the automated background tasks
  // such as update checks.
  virtual base::TimeDelta InitialDelay() const = 0;

  // Minimum amount of time the server needs to stay alive.
  virtual base::TimeDelta ServerKeepAliveTime() const = 0;

  // CRX format verification requirements.
  virtual crx_file::VerifierFormat CrxVerifierFormat() const = 0;

  // Overrides for the `GroupPolicyManager`.
  virtual base::Value::Dict GroupPolicies() const = 0;

  // Overrides the overinstall timeout.
  virtual base::TimeDelta OverinstallTimeout() const = 0;

  // Overrides the idleness check period.
  virtual base::TimeDelta IdleCheckPeriod() const = 0;

  // Overrides machine management state.
  virtual std::optional<bool> IsMachineManaged() const = 0;

  // True if the updater should request and apply diff updates.
  virtual bool EnableDiffUpdates() const = 0;

  // The maximum time allowed to establish a connection to CECA.
  virtual base::TimeDelta CecaConnectionTimeout() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<ExternalConstants>;
  scoped_refptr<ExternalConstants> next_provider_;
  virtual ~ExternalConstants();
};

// Sets up an external constants chain of responsibility. May block.
scoped_refptr<ExternalConstants> CreateExternalConstants();

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_H_
