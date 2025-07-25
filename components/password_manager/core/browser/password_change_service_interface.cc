// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_service_interface.h"

#include "base/command_line.h"
#include "components/password_manager/core/browser/password_manager_switches.h"

namespace password_manager {

GURL GetChangePasswordUrlOverride() {
  return GURL(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      kPasswordChangeUrl));
}

}  // namespace password_manager
