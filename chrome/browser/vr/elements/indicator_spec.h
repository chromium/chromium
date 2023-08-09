// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_INDICATOR_SPEC_H_
#define CHROME_BROWSER_VR_ELEMENTS_INDICATOR_SPEC_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

struct VR_UI_EXPORT IndicatorSpec {
  IndicatorSpec(UiElementName name,
                UiElementName webvr_name,
                const gfx::VectorIcon& icon,
                int resource_string,
                int background_resource_string,
                int potential_resource_string,
                CapturingStateModelMemberPtr signal);
  IndicatorSpec(const IndicatorSpec& other);
  ~IndicatorSpec();

  UiElementName name;
  UiElementName webvr_name;
  const raw_ref<const gfx::VectorIcon> icon;
  int resource_string;
  int background_resource_string;
  int potential_resource_string;
  CapturingStateModelMemberPtr signal;
};

VR_UI_EXPORT std::vector<IndicatorSpec> GetIndicatorSpecs();

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_INDICATOR_SPEC_H_
