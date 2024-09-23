// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/test/integration_test_commands.h"

namespace updater::test {

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommands() {
  return CreateIntegrationTestCommandsSystem();
}

}  // namespace updater::test
