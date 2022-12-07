// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/util/launchd_util.h"
#import "chrome/updater/util/mac_util.h"

namespace updater {

void AppInstall::WakeCandidateDone() {
  PollLaunchctlList(
      updater_scope(), GetUpdateServiceLaunchdName(updater_scope()),
      LaunchctlPresence::kPresent, base::Seconds(kWaitForLaunchctlUpdateSec),
      base::BindOnce([](scoped_refptr<AppInstall> installer,
                        bool unused) { installer->FetchPolicies(); },
                     base::WrapRefCounted(this)));
}

}  // namespace updater
