// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/common/telemetry_extension_converters.h"

#include <utility>

#include "base/notreached.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_exception.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"

namespace ash::converters {

namespace unchecked {

crosapi::mojom::TelemetryExtensionExceptionPtr UncheckedConvertPtr(
    cros_healthd::mojom::ExceptionPtr input) {
  return crosapi::mojom::TelemetryExtensionException::New(
      Convert(input->reason), input->debug_message);
}

crosapi::mojom::TelemetryExtensionSupportedPtr UncheckedConvertPtr(
    cros_healthd::mojom::SupportedPtr input) {
  return crosapi::mojom::TelemetryExtensionSupported::New();
}

crosapi::mojom::TelemetryExtensionUnsupportedReasonPtr UncheckedConvertPtr(
    cros_healthd::mojom::UnsupportedReasonPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::UnsupportedReason::Tag::kUnmappedUnionField:
      return crosapi::mojom::TelemetryExtensionUnsupportedReason::
          NewUnmappedUnionField(input->get_unmapped_union_field());
  }
}

crosapi::mojom::TelemetryExtensionUnsupportedPtr UncheckedConvertPtr(
    cros_healthd::mojom::UnsupportedPtr input) {
  return crosapi::mojom::TelemetryExtensionUnsupported::New(
      input->debug_message, ConvertCommonPtr(std::move(input->reason)));
}

crosapi::mojom::TelemetryExtensionSupportStatusPtr UncheckedConvertPtr(
    cros_healthd::mojom::SupportStatusPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::SupportStatus::Tag::kUnmappedUnionField:
      return crosapi::mojom::TelemetryExtensionSupportStatus::
          NewUnmappedUnionField(input->get_unmapped_union_field());
    case cros_healthd::mojom::SupportStatus::Tag::kException:
      return crosapi::mojom::TelemetryExtensionSupportStatus::NewException(
          ConvertCommonPtr(std::move(input->get_exception())));
    case cros_healthd::mojom::SupportStatus::Tag::kSupported:
      return crosapi::mojom::TelemetryExtensionSupportStatus::NewSupported(
          ConvertCommonPtr(std::move(input->get_supported())));
    case cros_healthd::mojom::SupportStatus::Tag::kUnsupported:
      return crosapi::mojom::TelemetryExtensionSupportStatus::NewUnsupported(
          ConvertCommonPtr(std::move(input->get_unsupported())));
  }
}

}  // namespace unchecked

crosapi::mojom::TelemetryExtensionException::Reason Convert(
    cros_healthd::mojom::Exception::Reason input) {
  switch (input) {
    case cros_healthd::mojom::Exception::Reason::kUnmappedEnumField:
      return crosapi::mojom::TelemetryExtensionException::Reason::
          kUnmappedEnumField;
    case cros_healthd::mojom::Exception::Reason::kMojoDisconnectWithoutReason:
      return crosapi::mojom::TelemetryExtensionException::Reason::
          kMojoDisconnectWithoutReason;
    case cros_healthd::mojom::Exception::Reason::kUnexpected:
      return crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected;
    case cros_healthd::mojom::Exception::Reason::kUnsupported:
      return crosapi::mojom::TelemetryExtensionException::Reason::kUnsupported;
    case cros_healthd::mojom::Exception::Reason::kCameraFrontendNotOpened:
      return crosapi::mojom::TelemetryExtensionException::Reason::
          kCameraFrontendNotOpened;
  }
  NOTREACHED();
}

}  // namespace ash::converters
