
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/security_state.h"

#ifndef COMPONENTS_SECURITY_STATE_CONTENT_ANDROID_SECURITY_STATE_MODEL_DELEGATE_H_
#define COMPONENTS_SECURITY_STATE_CONTENT_ANDROID_SECURITY_STATE_MODEL_DELEGATE_H_

namespace content {
class WebContents;
}  // namespace content

class SecurityStateModelDelegate {
 public:
  virtual ~SecurityStateModelDelegate() = default;
  virtual security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) const = 0;
};

#endif  // COMPONENTS_SECURITY_STATE_CONTENT_ANDROID_SECURITY_STATE_MODEL_DELEGATE_H_
