// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_SYSTEM_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_SYSTEM_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/test/integration_test_commands.h"

namespace updater {
namespace test {

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsSystem();

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_SYSTEM_H_
