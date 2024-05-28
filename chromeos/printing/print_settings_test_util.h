// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PRINT_SETTINGS_TEST_UTIL_H_
#define CHROMEOS_PRINTING_PRINT_SETTINGS_TEST_UTIL_H_

#include "chromeos/crosapi/mojom/print_preview_cros.mojom-forward.h"

// Util functions for testing print settings.
namespace chromeos {

crosapi::mojom::PrintSettingsPtr CreatePrintSettings(int preview_id);

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINT_SETTINGS_TEST_UTIL_H_
