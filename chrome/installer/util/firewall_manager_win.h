// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_FIREWALL_MANAGER_WIN_H_
#define CHROME_INSTALLER_UTIL_FIREWALL_MANAGER_WIN_H_

#include <memory>

namespace base {
class FilePath;
}

namespace installer {

// Requires that COM be initialized on the calling thread.
class FirewallManager {
 public:
  FirewallManager(const FirewallManager&) = delete;
  FirewallManager& operator=(const FirewallManager&) = delete;

  virtual ~FirewallManager();

  // Creates instance of |FirewallManager|. Implementation chooses best version
  // available for current version of Windows.
  static std::unique_ptr<FirewallManager> Create(
      const base::FilePath& chrome_path);

  // Returns true if application can one ports for incoming connections without
  // triggering firewall alert. It still does not guarantee that traffic
  // would pass firewall.
  virtual bool CanUseLocalPorts() = 0;

  // Installs all windows firewall rules needed by Chrome.
  // Return true if operation succeeded. Needs elevation.
  virtual bool AddFirewallRules() = 0;

  // Removes all windows firewall related to Chrome. Needs elevation.
  virtual void RemoveFirewallRules() = 0;

 protected:
  FirewallManager();
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_FIREWALL_MANAGER_WIN_H_
