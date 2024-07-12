// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/common/telemetry_extension_converters.h"

#include <cstdint>
#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::converters {

TEST(TelemetryExtensionConvertersTest, ConvertTelemetryExtensionException) {
  constexpr char kDebugMessage[] = "TestMessage";

  auto input = cros_healthd::mojom::Exception::New();
  input->reason = cros_healthd::mojom::Exception::Reason::kUnexpected;
  input->debug_message = kDebugMessage;

  auto result = ConvertCommonPtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->reason,
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);
  EXPECT_EQ(result->debug_message, kDebugMessage);
}

TEST(TelemetryExtensionConvertersTest, ConvertTelemetryExtensionSupportedPtr) {
  EXPECT_EQ(ConvertCommonPtr(cros_healthd::mojom::Supported::New()),
            crosapi::mojom::TelemetryExtensionSupported::New());
}

TEST(TelemetryExtensionConvertersTest,
     ConvertTelemetryExtensionUnsupportedReasonPtr) {
  EXPECT_EQ(
      ConvertCommonPtr(
          cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(9)),
      crosapi::mojom::TelemetryExtensionUnsupportedReason::
          NewUnmappedUnionField(9));
}

TEST(TelemetryExtensionConvertersTest,
     ConvertTelemetryExtensionUnsupportedPtr) {
  constexpr char kDebugMsg[] = "Test";
  constexpr uint8_t kUnmappedUnionField = 4;

  auto input = cros_healthd::mojom::Unsupported::New();
  input->debug_message = kDebugMsg;
  input->reason = cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(
      kUnmappedUnionField);

  auto result = ConvertCommonPtr(std::move(input));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->debug_message, kDebugMsg);
  EXPECT_EQ(result->reason,
            crosapi::mojom::TelemetryExtensionUnsupportedReason::
                NewUnmappedUnionField(kUnmappedUnionField));
}

TEST(TelemetryExtensionConvertersTest,
     ConvertTelemetryExtensionSupportStatusPtr) {
  constexpr char kDebugMsg[] = "Test";
  constexpr uint8_t kUnmappedUnionField = 4;

  EXPECT_EQ(ConvertCommonPtr(cros_healthd::mojom::SupportStatus::NewSupported(
                cros_healthd::mojom::Supported::New())),
            crosapi::mojom::TelemetryExtensionSupportStatus::NewSupported(
                crosapi::mojom::TelemetryExtensionSupported::New()));

  EXPECT_EQ(
      ConvertCommonPtr(
          cros_healthd::mojom::SupportStatus::NewUnmappedUnionField(
              kUnmappedUnionField)),
      crosapi::mojom::TelemetryExtensionSupportStatus::NewUnmappedUnionField(
          kUnmappedUnionField));

  auto unsupported = cros_healthd::mojom::Unsupported::New();
  unsupported->debug_message = kDebugMsg;
  unsupported->reason =
      cros_healthd::mojom::UnsupportedReason::NewUnmappedUnionField(
          kUnmappedUnionField);

  auto unsupported_result =
      ConvertCommonPtr(cros_healthd::mojom::SupportStatus::NewUnsupported(
          std::move(unsupported)));

  ASSERT_TRUE(unsupported_result->is_unsupported());
  EXPECT_EQ(unsupported_result->get_unsupported()->debug_message, kDebugMsg);
  EXPECT_EQ(unsupported_result->get_unsupported()->reason,
            crosapi::mojom::TelemetryExtensionUnsupportedReason::
                NewUnmappedUnionField(kUnmappedUnionField));

  auto exception = cros_healthd::mojom::Exception::New();
  exception->reason = cros_healthd::mojom::Exception::Reason::kUnexpected;
  exception->debug_message = kDebugMsg;

  auto exception_result = ConvertCommonPtr(
      cros_healthd::mojom::SupportStatus::NewException(std::move(exception)));

  ASSERT_TRUE(exception_result->is_exception());
  EXPECT_EQ(exception_result->get_exception()->reason,
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);
  EXPECT_EQ(exception_result->get_exception()->debug_message, kDebugMsg);
}

TEST(TelemetryExtensionConvertersTest,
     ConvertTelemetryExtensionExceptionReason) {
  EXPECT_EQ(
      Convert(cros_healthd::mojom::Exception::Reason::kUnmappedEnumField),
      crosapi::mojom::TelemetryExtensionException::Reason::kUnmappedEnumField);

  EXPECT_EQ(
      Convert(
          cros_healthd::mojom::Exception::Reason::kMojoDisconnectWithoutReason),
      crosapi::mojom::TelemetryExtensionException::Reason::
          kMojoDisconnectWithoutReason);

  EXPECT_EQ(Convert(cros_healthd::mojom::Exception::Reason::kUnexpected),
            crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected);

  EXPECT_EQ(Convert(cros_healthd::mojom::Exception::Reason::kUnsupported),
            crosapi::mojom::TelemetryExtensionException::Reason::kUnsupported);

  EXPECT_EQ(
      Convert(cros_healthd::mojom::Exception::Reason::kCameraFrontendNotOpened),
      crosapi::mojom::TelemetryExtensionException::Reason::
          kCameraFrontendNotOpened);
}

}  // namespace ash::converters
