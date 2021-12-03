// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/setup.h"

#include "base/callback.h"
#include "base/notreached.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

void InstallCandidate(UpdaterScope /*scope*/,
                      base::OnceCallback<void(int)> /*callback*/) {
  // TODO(crbug.com/1276176) - implement.
  NOTIMPLEMENTED();
}

}  // namespace updater
