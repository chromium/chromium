// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBNN_WIN_APP_RUNTIME_INSTALLER_H_
#define CHROME_BROWSER_WEBNN_WIN_APP_RUNTIME_INSTALLER_H_

namespace webnn {

// Schedules the Windows App Runtime package installation when both the
// `kWebMachineLearningNeuralNetwork` and `kWebNNOnnxRuntime` features are
// enabled, and the OS is Windows 11 version 24H2 or later.
//
// This function uses the IAppInstallManager API to install the Windows App
// Runtime package. After installation, a package dependency is created using
// Chromium's user data directory to prevent the OS from cleaning up the
// package. The dependency ID and package information will be stored in user
// settings.
void SchedulePlatformRuntimeInstallationIfRequired();

}  // namespace webnn

#endif  // CHROME_BROWSER_WEBNN_WIN_APP_RUNTIME_INSTALLER_H_
