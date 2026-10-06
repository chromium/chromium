// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_PLATFORM_RUNTIME_COMPONENT_H_
#define CHROME_INSTALLER_SETUP_PLATFORM_RUNTIME_COMPONENT_H_

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "chrome/installer/setup/component_interface.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class DictValue;
}  // namespace base

namespace installer {

class InstallerState;

// Component implementation for the Chrome Platform Runtime inner component,
// which is installed into a directory named for its CRX ID within the
// installation's target directory.
class PlatformRuntimeComponent : public ComponentInterface {
 public:
  // Returns a new instance. The component takes no input from `command_line`.
  static std::unique_ptr<PlatformRuntimeComponent> Create(
      const base::CommandLine& command_line,
      const InstallerState& installer_state);

  PlatformRuntimeComponent();
  PlatformRuntimeComponent(const PlatformRuntimeComponent&) = delete;
  PlatformRuntimeComponent& operator=(const PlatformRuntimeComponent&) = delete;
  ~PlatformRuntimeComponent() override;

  // ComponentInterface:
  bool ReadManifest(const base::DictValue& manifest) override;
  base::expected<base::FilePath, InstallStatus> DetermineDestinationRoot(
      const InstallerState& installer_state,
      std::string_view crx_id) override;
  bool InitializeDestinationRoot(
      const InstallerState& installer_state,
      const base::FilePath& destination_root) override;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_PLATFORM_RUNTIME_COMPONENT_H_
