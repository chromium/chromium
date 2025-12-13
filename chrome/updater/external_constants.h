// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"

class GURL;

namespace base {
class TimeDelta;
}

namespace crx_file {
enum class VerifierFormat;
}

namespace updater {

struct EventLoggingPermissionProvider {
 public:
  std::string app_id;
#if BUILDFLAG(IS_MAC)
  // On macOS, in addition to an AppId the application's directory name relative
  // to Application Support/COMPANY_SHORTNAME_STRING is needed to determine
  // whether event logging is allowed.
  std::string directory_name;
#endif
};

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

  // The URL for the app logos.
  virtual GURL AppLogoURL() const = 0;

  // The URL for remote event logging.
  virtual GURL EventLoggingURL() const = 0;

  // True if client update protocol signing of update checks is enabled.
  virtual bool UseCUP() const = 0;

  // Time to delay the start of the automated background tasks
  // such as update checks.
  virtual base::TimeDelta InitialDelay() const = 0;

  // Minimum amount of time the server needs to stay alive.
  virtual base::TimeDelta ServerKeepAliveTime() const = 0;

  // CRX format verification requirements.
  virtual crx_file::VerifierFormat CrxVerifierFormat() const = 0;

  // Required CRX key (this is a SHA256 hash of the public key).
  virtual std::optional<std::vector<uint8_t>> CrxPublicKeyHash() const = 0;

  // Minimum amount of time between successive event logging transmissions.
  virtual base::TimeDelta MinimumEventLoggingCooldown() const = 0;

  // Indicates which application remote event logging permissions should be
  // inferred from. Nullopt indicates that logging is unconditionally disabled.
  virtual std::optional<EventLoggingPermissionProvider>
  GetEventLoggingPermissionProvider() const = 0;

  // Policies for the `PolicyManager` surfaced by external constants.
  virtual base::Value::Dict DictPolicies() const = 0;

  // Overrides the overinstall timeout.
  virtual base::TimeDelta OverinstallTimeout() const = 0;

  // Overrides the idleness check period.
  virtual base::TimeDelta IdleCheckPeriod() const = 0;

  // Overrides machine management state.
  virtual std::optional<bool> IsMachineManaged() const = 0;

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
