// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_MOJOM_TRAITS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_MOJOM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "components/attribution_reporting/os_registration_error.mojom-forward.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/registration_header_error.mojom-shared.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(
    ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_MOJOM_TRAITS)
    StructTraits<attribution_reporting::mojom::RegistrationHeaderErrorDataView,
                 attribution_reporting::RegistrationHeaderError> {
  static const std::string& header_value(
      const attribution_reporting::RegistrationHeaderError& error) {
    return error.header_value;
  }

  static attribution_reporting::RegistrationHeaderErrorDetails error_details(
      const attribution_reporting::RegistrationHeaderError& error) {
    return error.error_details;
  }

  static bool Read(
      attribution_reporting::mojom::RegistrationHeaderErrorDataView data,
      attribution_reporting::RegistrationHeaderError* out);
};

template <>
struct COMPONENT_EXPORT(
    ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_MOJOM_TRAITS)
    UnionTraits<
        attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView,
        attribution_reporting::RegistrationHeaderErrorDetails> {
  static attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView::
      Tag
      GetTag(
          const attribution_reporting::RegistrationHeaderErrorDetails& details);

  static attribution_reporting::mojom::SourceRegistrationError source_error(
      const attribution_reporting::RegistrationHeaderErrorDetails& details) {
    return absl::get<attribution_reporting::mojom::SourceRegistrationError>(
        details);
  }

  static attribution_reporting::mojom::TriggerRegistrationError trigger_error(
      const attribution_reporting::RegistrationHeaderErrorDetails& details) {
    return absl::get<attribution_reporting::mojom::TriggerRegistrationError>(
        details);
  }

  static attribution_reporting::mojom::OsRegistrationError os_source_error(
      const attribution_reporting::RegistrationHeaderErrorDetails& details) {
    return *absl::get<attribution_reporting::OsSourceRegistrationError>(
        details);
  }

  static attribution_reporting::mojom::OsRegistrationError os_trigger_error(
      const attribution_reporting::RegistrationHeaderErrorDetails& details) {
    return *absl::get<attribution_reporting::OsTriggerRegistrationError>(
        details);
  }

  static bool Read(
      attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView data,
      attribution_reporting::RegistrationHeaderErrorDetails* out);
};

}  // namespace mojo

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_REGISTRATION_HEADER_ERROR_MOJOM_TRAITS_H_
