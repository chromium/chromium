// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBNN_WIN_APP_RUNTIME_INSTALLER_H_
#define CHROME_BROWSER_WEBNN_WIN_APP_RUNTIME_INSTALLER_H_

namespace webnn {

// Schedules the installation of the Windows App Runtime package if it was not
// deployed by the Chrome installer.
//
// Triggers only on Windows 11 24H2+ when both
// `kWebMachineLearningNeuralNetwork` and `kWebNNOnnxRuntime` features are
// enabled.
//
// Upon successful installation, a package dependency is created using the
// Chromium user data directory to prevent the OS from removing the package.
// The dependency ID and package metadata are persisted in user settings.
void SchedulePlatformRuntimeInstallationIfRequired();

}  // namespace webnn

#endif  // CHROME_BROWSER_WEBNN_WIN_APP_RUNTIME_INSTALLER_H_
