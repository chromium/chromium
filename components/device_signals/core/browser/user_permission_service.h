// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_H_

#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"

namespace device_signals {

struct UserContext;

// Contains possible outcomes of a signals collection permission check.
// These values are persisted to logs and should not be renumbered. Please
// update the DeviceSignalsUserPermission enum in enums.xml when adding a new
// value here.
enum class UserPermission {
  // Returned when the user is part of an organization that is not affiliated
  // with the organization currently managing the browser.
  kUnaffiliated = 0,

  // Returned when the browser is not managed, but the user is - but the user
  // has not given their consent for device signals to be collected.
  kMissingConsent = 1,

  // Returned when the user is not part of any organization.
  kConsumerUser = 2,

  // Returned when the given user context does not represent the current browser
  // user (e.g. Profile user).
  kUnknownUser = 3,

  // Returned when the no user information was given.
  kMissingUser = 4,

  // Returned when the user is granted permission to the device's signals.
  kGranted = 5,

  // Returned when the current context is currently unsupported, but eventually
  // could be.
  kUnsupported = 6,

  kMaxValue = kUnsupported
};

// Service that can be used to conduct permission checks on given users. The
// users may represent a different user than the profile user, and so the
// permission check is more exhaustive than simple consent check and involves
// validating the affiliation of the user's organization.
class UserPermissionService : public KeyedService {
 public:
  ~UserPermissionService() override = default;

  // Returns true if consent is required based on the current context and is
  // missing.
  virtual bool ShouldCollectConsent() const = 0;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Will verify whether context-aware signals can be collected
  // on behalf of the user represented by `user_context`. Returns `kGranted` if
  // collection is allowed.
  virtual UserPermission CanUserCollectSignals(
      const UserContext& user_context) const = 0;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX

  // Will verify whether context-aware signals can be collected
  // based on the current context (e.g. browser-wide management, user logged-in
  // to a Profile). Returns `kGranted` if collection is allowed.
  virtual UserPermission CanCollectSignals() const = 0;

  // Returns whether the user has explicitly agreed to device signals being
  // shared or not. Depending on the current management context, the returned
  // value could be false even though signals can be collected. This function
  // is exposed publicly mostly for debugging purposes.
  virtual bool HasUserConsented() const = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_H_
