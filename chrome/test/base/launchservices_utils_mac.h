// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_LAUNCHSERVICES_UTILS_MAC_H_
#define CHROME_TEST_BASE_LAUNCHSERVICES_UTILS_MAC_H_

namespace test {

// Attempts to guess the path to the Chromium app bundle and register it with
// LaunchServices. This is necessary in tests that want to install protocol
// handlers, since as of macOS 10.15 a bundle ID cannot be the handler for a
// protocol unless a corresponding app is already registered with
// LaunchServices.
//
// Note that there is no exposed LaunchServices API to unregister an app, so
// there is no way to undo this, but registering again will replace the old
// registration.
//
// Returns true if LaunchServices claimed that the registration succeeded. Note
// that success does not necessarily mean the app was successfully registered
// since part of the registration process is asynchronous.
bool RegisterAppWithLaunchServices();

}  // namespace test

#endif  // CHROME_TEST_BASE_LAUNCHSERVICES_UTILS_MAC_H_
