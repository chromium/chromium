// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_TEST_SCOPE_H_
#define CHROME_UPDATER_TEST_TEST_SCOPE_H_

namespace updater {

enum class UpdaterScope;

UpdaterScope GetUpdaterScopeForTesting();

}  // namespace updater

#endif  // CHROME_UPDATER_TEST_TEST_SCOPE_H_
