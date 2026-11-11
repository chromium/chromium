// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_SWITCHES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_SWITCHES_H_

namespace switches {

// Overrides the default endpoint URL for the Passbox credential store service.
extern const char kPassboxEndpoint[];

// Overrides the default endpoint URL for the Agentic Permission Service (APS).
extern const char kAgentPermissionServiceEndpoint[];

}  // namespace switches

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_SWITCHES_H_
