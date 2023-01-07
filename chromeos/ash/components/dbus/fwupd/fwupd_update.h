// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_UPDATE_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_UPDATE_H_

#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace ash {

// Structure to hold update details received from fwupd.
struct COMPONENT_EXPORT(ASH_DBUS_FWUPD) FwupdUpdate {
  FwupdUpdate();
  FwupdUpdate(const std::string& version,
              const std::string& description,
              int priority,
              const base::FilePath& filename,
              const std::string& checksum);
  FwupdUpdate(FwupdUpdate&& other);
  FwupdUpdate& operator=(FwupdUpdate&& other);
  ~FwupdUpdate();

  std::string version;
  std::string description;
  int priority;
  base::FilePath filepath;
  std::string checksum;
};

using FwupdUpdateList = std::vector<FwupdUpdate>;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FWUPD_UPDATE_H_
