// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_PUBLIC_CPP_UIA_ELEMENT_PROBE_TYPES_H_
#define CHROME_SERVICES_UTIL_WIN_PUBLIC_CPP_UIA_ELEMENT_PROBE_TYPES_H_

#include <stdint.h>

#include <string>

#include "base/process/process.h"
#include "base/time/time.h"
#include "base/win/windows_types.h"
#include "ui/gfx/geometry/rect.h"

// The C++ forms of the structs in uia_element_probe.mojom, which documents
// each field. Both the UiaElementProbe service and its browser-side client use
// these; uia_element_probe_mojom_traits.h carries them across the pipe.

// chrome.mojom.WindowIdentity.
struct UiaWindowIdentity {
  HWND hwnd = nullptr;
  // Opened with at least PROCESS_QUERY_LIMITED_INFORMATION.
  base::Process process{};
  std::wstring window_class;
};

// chrome.mojom.UiaElementQuery.
struct UiaElementQuery {
  std::wstring automation_id;
  std::wstring fallback_automation_id_fragment;
  int32_t fallback_control_type = 0;
  base::TimeDelta wait_for;
};

// chrome.mojom.UiaElementMeasurement.
struct UiaElementMeasurement {
  gfx::Rect element_bounds;
  gfx::Rect window_bounds;
};

#endif  // CHROME_SERVICES_UTIL_WIN_PUBLIC_CPP_UIA_ELEMENT_PROBE_TYPES_H_
