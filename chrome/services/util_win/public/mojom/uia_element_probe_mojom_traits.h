// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UIA_ELEMENT_PROBE_MOJOM_TRAITS_H_
#define CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UIA_ELEMENT_PROBE_MOJOM_TRAITS_H_

#include <stdint.h>

#include <string>

#include "base/time/time.h"
#include "chrome/services/util_win/public/cpp/uia_element_probe_types.h"
#include "chrome/services/util_win/public/mojom/uia_element_probe.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {

template <>
struct StructTraits<chrome::mojom::WindowIdentityDataView, UiaWindowIdentity> {
  static uint32_t hwnd(const UiaWindowIdentity& input);
  // Takes the process handle: the struct is move-only on the wire.
  static PlatformHandle process(UiaWindowIdentity& input);
  static const std::wstring& window_class(const UiaWindowIdentity& input) {
    return input.window_class;
  }

  static bool Read(chrome::mojom::WindowIdentityDataView data,
                   UiaWindowIdentity* output);
};

template <>
struct StructTraits<chrome::mojom::UiaElementQueryDataView, UiaElementQuery> {
  static const std::wstring& automation_id(const UiaElementQuery& input) {
    return input.automation_id;
  }
  static const std::wstring& fallback_automation_id_fragment(
      const UiaElementQuery& input) {
    return input.fallback_automation_id_fragment;
  }
  static int32_t fallback_control_type(const UiaElementQuery& input) {
    return input.fallback_control_type;
  }
  static base::TimeDelta wait_for(const UiaElementQuery& input) {
    return input.wait_for;
  }

  static bool Read(chrome::mojom::UiaElementQueryDataView data,
                   UiaElementQuery* output);
};

template <>
struct StructTraits<chrome::mojom::UiaElementMeasurementDataView,
                    UiaElementMeasurement> {
  static const gfx::Rect& element_bounds(const UiaElementMeasurement& input) {
    return input.element_bounds;
  }
  static const gfx::Rect& window_bounds(const UiaElementMeasurement& input) {
    return input.window_bounds;
  }

  static bool Read(chrome::mojom::UiaElementMeasurementDataView data,
                   UiaElementMeasurement* output);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UIA_ELEMENT_PROBE_MOJOM_TRAITS_H_
