// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

namespace updater {

void AppInstall::WakeCandidateDone() {
  FetchPolicies();
}

}  // namespace updater
