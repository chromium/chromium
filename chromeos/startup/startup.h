// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_STARTUP_STARTUP_H_
#define CHROMEOS_STARTUP_STARTUP_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace chromeos {

// Reads the startup data. The FD to be read for the startup data should be
// specified via the kCrosStartupDataFD command line flag. This function
// consumes the FD, so this must not be called twice in a process.
COMPONENT_EXPORT(CHROMEOS_STARTUP)
std::optional<std::string> ReadStartupData();

// Creates a memory backed file containing the serialized |params|,
// and returns its FD.
COMPONENT_EXPORT(CHROMEOS_STARTUP)
base::ScopedFD CreateMemFDFromBrowserInitParams(
    const crosapi::mojom::BrowserInitParamsPtr& data);

}  // namespace chromeos

#endif  // CHROMEOS_STARTUP_STARTUP_H_
