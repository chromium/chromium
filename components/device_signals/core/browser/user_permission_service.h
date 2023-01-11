// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_H_

#include "base/functional/callback_forward.h"
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

  kMaxValue = kGranted
};

// Service that can be used to conduct permission checks on given users. The
// users may represent a different user than the profile user, and so the
// permission check is more exhaustive than simple consent check and involves
// validating the affiliation of the user's organization.
class UserPermissionService : public KeyedService {
 public:
  using CanCollectCallback = base::OnceCallback<void(UserPermission)>;

  ~UserPermissionService() override = default;

  // Will asynchronously verify whether context-aware signals can be collected
  // on behalf of the user represented by `user_context`. The determined user
  // permission is returned via `callback`.
  virtual void CanCollectSignals(const UserContext& user_context,
                                 CanCollectCallback callback) = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_H_
