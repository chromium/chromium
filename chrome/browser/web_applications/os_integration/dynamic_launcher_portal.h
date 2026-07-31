// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_DYNAMIC_LAUNCHER_PORTAL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_DYNAMIC_LAUNCHER_PORTAL_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "dbus/bus.h"
#include "url/gurl.h"

namespace web_app {

inline constexpr char kDynamicLauncherInterfaceName[] =
    "org.freedesktop.portal.DynamicLauncher";

// Helper class to communicate with the org.freedesktop.portal.DynamicLauncher
// interface, allowing sandboxed applications to dynamically create and
// remove desktop launcher shortcuts.
class DynamicLauncherPortal {
 public:
  using InstallCallback = base::OnceCallback<void(bool success)>;
  using PrepareInstallCallback =
      base::OnceCallback<void(std::optional<std::string> token)>;
  using UninstallCallback = base::OnceCallback<void(bool success)>;

  explicit DynamicLauncherPortal(scoped_refptr<dbus::Bus> bus = nullptr);
  ~DynamicLauncherPortal();

  DynamicLauncherPortal(const DynamicLauncherPortal&) = delete;
  DynamicLauncherPortal& operator=(const DynamicLauncherPortal&) = delete;

  // Asynchronously checks if the DynamicLauncher portal is available.
  void IsAvailable(base::OnceCallback<void(bool)> callback);

  // Asynchronously requests permission to install a dynamic launcher shortcut.
  // Returns an installation token via `callback` on success.
  void PrepareInstall(const std::string& name,
                      const std::vector<uint8_t>& icon_bytes,
                      const GURL& target_url,
                      PrepareInstallCallback callback);

  // Asynchronously installs a dynamic launcher desktop entry.
  // `token` is the token returned by PrepareInstall (or an empty string).
  // `desktop_file_id` is the filename for the desktop entry (e.g.
  // "app.desktop"). `desktop_entry` is the content of the .desktop file.
  void Install(const std::string& token,
               const std::string& desktop_file_id,
               const std::string& desktop_entry,
               InstallCallback callback);

  // Asynchronously uninstalls a dynamic launcher shortcut.
  void Uninstall(const std::string& desktop_file_id,
                 UninstallCallback callback);

  // Resets availability cache between unit tests.
  static void ResetAvailabilityCacheForTesting();

 private:
  scoped_refptr<dbus::Bus> bus_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_DYNAMIC_LAUNCHER_PORTAL_H_
