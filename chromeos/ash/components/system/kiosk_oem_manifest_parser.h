// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_KIOSK_OEM_MANIFEST_PARSER_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_KIOSK_OEM_MANIFEST_PARSER_H_

#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ash {

// Parser for app kiosk OEM manifest files.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM) KioskOemManifestParser {
 public:
  // Kiosk OEM manifest.
  struct Manifest {
    Manifest();

    // True if OOBE flow should enforce enterprise enrollment.
    bool enterprise_managed;
    // True user can exit enterprise enrollment during OOBE.
    bool can_exit_enrollment;
    // Intended purpose of the device. Meant to be pass-through value for
    // enterprise enrollment.
    std::string device_requisition;
    // True if OOBE flow should be adapted for keyboard flow.
    bool keyboard_driven_oobe;
  };

  KioskOemManifestParser() = delete;
  KioskOemManifestParser(const KioskOemManifestParser&) = delete;
  KioskOemManifestParser& operator=(const KioskOemManifestParser&) = delete;

  // Loads manifest from |kiosk_oem_file|. Returns true if manifest was
  // found and successfully parsed.
  static bool Load(const base::FilePath& kiosk_oem_file, Manifest* manifest);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_KIOSK_OEM_MANIFEST_PARSER_H_
