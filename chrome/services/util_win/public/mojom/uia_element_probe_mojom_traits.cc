// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/public/mojom/uia_element_probe_mojom_traits.h"

#include <utility>

#include "base/process/process.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_handle_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/base/wstring_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// static
uint32_t StructTraits<chrome::mojom::WindowIdentityDataView,
                      UiaWindowIdentity>::hwnd(const UiaWindowIdentity& input) {
  return base::win::HandleToUint32(input.hwnd);
}

// static
PlatformHandle
StructTraits<chrome::mojom::WindowIdentityDataView, UiaWindowIdentity>::process(
    UiaWindowIdentity& input) {
  return PlatformHandle(base::win::ScopedHandle(input.process.Release()));
}

// static
bool StructTraits<chrome::mojom::WindowIdentityDataView,
                  UiaWindowIdentity>::Read(chrome::mojom::WindowIdentityDataView
                                               data,
                                           UiaWindowIdentity* output) {
  // The field is non-nullable, so Mojo has already rejected a missing handle.
  PlatformHandle process = data.TakeProcess();
  if (!process.is_valid()) {
    return false;
  }
  output->hwnd = reinterpret_cast<HWND>(base::win::Uint32ToHandle(data.hwnd()));
  output->process = base::Process(process.TakeHandle().Take());
  return data.ReadWindowClass(&output->window_class);
}

// static
bool StructTraits<chrome::mojom::UiaElementQueryDataView, UiaElementQuery>::
    Read(chrome::mojom::UiaElementQueryDataView data, UiaElementQuery* output) {
  output->fallback_control_type = data.fallback_control_type();
  if (!data.ReadAutomationId(&output->automation_id) ||
      !data.ReadFallbackAutomationIdFragment(
          &output->fallback_automation_id_fragment) ||
      !data.ReadWaitFor(&output->wait_for)) {
    return false;
  }
  // A query has to name something to find.
  return !output->automation_id.empty() ||
         !output->fallback_automation_id_fragment.empty();
}

// static
bool StructTraits<chrome::mojom::UiaElementMeasurementDataView,
                  UiaElementMeasurement>::
    Read(chrome::mojom::UiaElementMeasurementDataView data,
         UiaElementMeasurement* output) {
  return data.ReadElementBounds(&output->element_bounds) &&
         data.ReadWindowBounds(&output->window_bounds);
}

}  // namespace mojo
