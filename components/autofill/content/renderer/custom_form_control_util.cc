// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_util.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace autofill {

base::flat_map<blink::WebFormElement, std::vector<CustomFormControl>>
GroupCustomFormControlsByForm(std::vector<CustomFormControl> controls) {
  base::flat_map<blink::WebFormElement, std::vector<CustomFormControl>> result;
  for (CustomFormControl& control : controls) {
    blink::WebFormElement form =
        form_util::GetOutermostAncestorFormElement(control.canonical_dom_node);
    result[form].push_back(std::move(control));
  }
  return result;
}

}  // namespace autofill
