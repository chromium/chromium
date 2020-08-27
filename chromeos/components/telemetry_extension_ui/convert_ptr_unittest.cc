// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/convert_ptr.h"

#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace converters {

// Tests that |ConvertPtr| function returns nullptr if input is nullptr.
// ConvertPtr is a template, so we can test this function with any valid type.
TEST(TelemetryConvertPtr, ConvertPtrTakesNullPtr) {
  EXPECT_TRUE(ConvertPtr(cros_healthd::mojom::ProbeErrorPtr()).is_null());
}

}  // namespace converters
}  // namespace chromeos