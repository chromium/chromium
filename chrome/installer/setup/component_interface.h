// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_COMPONENT_INTERFACE_H_
#define CHROME_INSTALLER_SETUP_COMPONENT_INTERFACE_H_

#include <string_view>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class DictValue;
}  // namespace base

namespace installer {

class InstallerState;

// Interface representing a component supported by the installer. Encapsulates
// component-specific manifest validation, destination root determination, and
// root initialization.
class ComponentInterface {
 public:
  virtual ~ComponentInterface() = default;

  // Reads and validates component-specific manifest contents (e.g. expected
  // name and other required fields), retaining any values needed by subsequent
  // operations. Returns false if the manifest is invalid. Must be called before
  // any other method.
  virtual bool ReadManifest(const base::DictValue& manifest) = 0;

  // Determines the destination root directory for this component. If the root
  // directory cannot be determined or prerequisites are unmet, returns the
  // status with which installation is to fail.
  virtual base::expected<base::FilePath, InstallStatus>
  DetermineDestinationRoot(const InstallerState& installer_state,
                           std::string_view crx_id) = 0;

  // Performs any component-specific initialization on the destination root
  // directory. Returns true on success or if no initialization is needed.
  virtual bool InitializeDestinationRoot(
      const InstallerState& installer_state,
      const base::FilePath& destination_root) = 0;

 protected:
  ComponentInterface() = default;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_COMPONENT_INTERFACE_H_
