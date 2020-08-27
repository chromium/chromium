// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_TEST_APP_TEST_APP_H_
#define CHROME_UPDATER_TEST_TEST_APP_TEST_APP_H_

namespace updater {

// Installs the updater.
int InstallUpdater();

int TestAppMain(int argc, const char** argv);

}  // namespace updater

#endif  // CHROME_UPDATER_TEST_TEST_APP_TEST_APP_H_
