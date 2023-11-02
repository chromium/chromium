// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fwupd/fwupd_update.h"

#include "base/files/file_path.h"

namespace ash {

FwupdUpdate::FwupdUpdate() = default;

FwupdUpdate::FwupdUpdate(const std::string& version,
                         const std::string& description,
                         int priority,
                         const base::FilePath& filepath,
                         const std::string& checksum)
    : version(version),
      description(description),
      priority(priority),
      filepath(filepath),
      checksum(checksum) {}

FwupdUpdate::FwupdUpdate(FwupdUpdate&& other) = default;
FwupdUpdate& FwupdUpdate::operator=(FwupdUpdate&& other) = default;
FwupdUpdate::~FwupdUpdate() = default;

}  // namespace ash
