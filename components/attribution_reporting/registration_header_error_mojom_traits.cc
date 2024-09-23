// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_header_error_mojom_traits.h"

#include "base/functional/overloaded.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/registration_header_error.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo {

// static
bool StructTraits<attribution_reporting::mojom::RegistrationHeaderErrorDataView,
                  attribution_reporting::RegistrationHeaderError>::
    Read(attribution_reporting::mojom::RegistrationHeaderErrorDataView data,
         attribution_reporting::RegistrationHeaderError* out) {
  if (!data.ReadHeaderValue(&out->header_value)) {
    return false;
  }

  if (!data.ReadErrorDetails(&out->error_details)) {
    return false;
  }

  return true;
}

// static
attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView::Tag
UnionTraits<
    attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView,
    attribution_reporting::RegistrationHeaderErrorDetails>::
    GetTag(
        const attribution_reporting::RegistrationHeaderErrorDetails& details) {
  return absl::visit(
      base::Overloaded{
          [](attribution_reporting::mojom::SourceRegistrationError) {
            return attribution_reporting::mojom::
                RegistrationHeaderErrorDetailsDataView::Tag::kSourceError;
          },

          [](attribution_reporting::mojom::TriggerRegistrationError) {
            return attribution_reporting::mojom::
                RegistrationHeaderErrorDetailsDataView::Tag::kTriggerError;
          },

          [](attribution_reporting::OsSourceRegistrationError) {
            return attribution_reporting::mojom::
                RegistrationHeaderErrorDetailsDataView::Tag::kOsSourceError;
          },

          [](attribution_reporting::OsTriggerRegistrationError) {
            return attribution_reporting::mojom::
                RegistrationHeaderErrorDetailsDataView::Tag::kOsTriggerError;
          },
      },
      details);
}

// static
bool UnionTraits<
    attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView,
    attribution_reporting::RegistrationHeaderErrorDetails>::
    Read(attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView
             data,
         attribution_reporting::RegistrationHeaderErrorDetails* out) {
  switch (data.tag()) {
    case attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView::
        Tag::kSourceError:
      attribution_reporting::mojom::SourceRegistrationError source_error;
      if (!data.ReadSourceError(&source_error)) {
        return false;
      }
      *out = source_error;
      return true;
    case attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView::
        Tag::kTriggerError:
      attribution_reporting::mojom::TriggerRegistrationError trigger_error;
      if (!data.ReadTriggerError(&trigger_error)) {
        return false;
      }
      *out = trigger_error;
      return true;
    case attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView::
        Tag::kOsSourceError:
      attribution_reporting::mojom::OsRegistrationError os_source_error;
      if (!data.ReadOsSourceError(&os_source_error)) {
        return false;
      }
      *out = attribution_reporting::OsSourceRegistrationError(os_source_error);
      return true;
    case attribution_reporting::mojom::RegistrationHeaderErrorDetailsDataView::
        Tag::kOsTriggerError:
      attribution_reporting::mojom::OsRegistrationError os_trigger_error;
      if (!data.ReadOsTriggerError(&os_trigger_error)) {
        return false;
      }
      *out =
          attribution_reporting::OsTriggerRegistrationError(os_trigger_error);
      return true;
  }

  return false;
}

}  // namespace mojo
