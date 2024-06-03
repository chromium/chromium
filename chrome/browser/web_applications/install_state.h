// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_INSTALL_STATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_INSTALL_STATE_H_

#include <iosfwd>

namespace base {
template <typename E, E MinEnumValue, E MaxEnumValue>
class EnumSet;
}

namespace web_app {

// The installation state a web app can be in. These states are exclusive - a
// web app can only be one of these.
// TODO(crbug.com/340952021): Use the new proto InstallState when available.
enum class InstallState {
  // Formerly "not locally installed", the app is installed on another device
  // but not on this device. This app is in registry and installed via sync and
  // will have basic information like the name and icons downloaded. This app
  // has no OS integration and cannot be launched in standalone mode without
  // being automatically upgraded to `kInstalledWithOsIntegration` and having
  // all OS integration installed.
  kSuggestedFromAnotherDevice,

  // The app is installed on this device, but has not done OS integration like
  // create shortcuts, register file handlers, etc. This app cannot be launched
  // in standalone mode without being automatically upgraded to
  // `kInstalledWithOsIntegration`  and having all OS integration installed.
  kInstalledWithoutOsIntegration,

  kInstalledWithOsIntegration,
  kMaxValue = kInstalledWithOsIntegration
};

// Defined here to ensure the min and max values are updated.
using InstallStateSet = base::EnumSet<InstallState,
                                      InstallState::kSuggestedFromAnotherDevice,
                                      InstallState::kMaxValue>;

std::ostream& operator<<(std::ostream&, const InstallState& state);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_INSTALL_STATE_H_
