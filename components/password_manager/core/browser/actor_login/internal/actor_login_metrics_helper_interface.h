// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_HELPER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_HELPER_INTERFACE_H_

namespace actor_login {

// Interface for recording Actor.Login metrics.
class ActorLoginMetricsHelperInterface {
 public:
  virtual ~ActorLoginMetricsHelperInterface() = default;

  // Records whether deduplication occurred during a get credentials request.
  virtual void RecordDeduplicationOccurred(bool deduplication_occurred) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_HELPER_INTERFACE_H_
