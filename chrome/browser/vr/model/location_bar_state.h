// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_LOCATION_BAR_STATE_H_
#define CHROME_BROWSER_VR_MODEL_LOCATION_BAR_STATE_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace gfx {
struct VectorIcon;
}

namespace vr {

// Passes information obtained from LocationBarModel to the VR UI framework.
struct VR_BASE_EXPORT LocationBarState {
 public:
  LocationBarState();
  LocationBarState(const GURL& url,
                   security_state::SecurityLevel level,
                   const gfx::VectorIcon* icon,
                   bool display_url,
                   bool offline);
  LocationBarState(const LocationBarState& other);
  LocationBarState& operator=(const LocationBarState& other);

  bool operator==(const LocationBarState& other) const;
  bool operator!=(const LocationBarState& other) const;

  GURL gurl;
  security_state::SecurityLevel security_level;
  // This field is not a raw_ptr<> because of problems related to lambdas with
  // no return type, where the return value is raw_ptr<T>, but
  // variable/parameter receiving the lambda expects the raw pointer type.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* vector_icon;
  bool should_display_url;
  bool offline_page;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_LOCATION_BAR_STATE_H_
