// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_DEFAULT_SESSION_CONTROLLER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_DEFAULT_SESSION_CONTROLLER_H_

#include "components/feature_engagement/public/session_controller.h"

namespace feature_engagement {

class DefaultSessionController : public SessionController {
 public:
  explicit DefaultSessionController();
  ~DefaultSessionController() override;

  bool ShouldResetSession() override;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_DEFAULT_SESSION_CONTROLLER_H_
