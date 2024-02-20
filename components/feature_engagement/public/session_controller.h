// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SESSION_CONTROLLER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SESSION_CONTROLLER_H_

namespace feature_engagement {

// A base class that feature engagement uses for controlling the session's
// lifetime. By default platforms don't need to implement this class. A default
// implementation is used.
class SessionController {
 public:
  virtual ~SessionController() = default;
  SessionController(const SessionController&) = delete;
  void operator=(const SessionController&) = delete;

  // If returns true, we assume the session will be reset and the session start
  // time will be set to the current time.
  virtual bool ShouldResetSession() = 0;

 protected:
  SessionController() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_SESSION_CONTROLLER_H_
