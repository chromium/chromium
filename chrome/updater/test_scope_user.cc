// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test_scope.h"

#include "chrome/updater/updater_scope.h"

namespace updater {

UpdaterScope GetTestScope() {
  return UpdaterScope::kUser;
}

}  // namespace updater
