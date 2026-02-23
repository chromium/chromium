// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_TEST_API_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "url/gurl.h"

namespace autofill {

class FormTrackerTestApi {
 public:
  explicit FormTrackerTestApi(FormTracker* agent) : form_tracker_(*agent) {}

  void DidFinishSameDocumentNavigation() {
    return form_tracker_->DidFinishSameDocumentNavigation();
  }

  void DidStartNavigation(
      std::optional<blink::WebNavigationType> navigation_type) {
    form_tracker_->DidStartNavigation(GURL(), navigation_type);
  }

  std::optional<FormData> provisionally_saved_form() {
    return form_tracker_->provisionally_saved_form();
  }

 private:
  const raw_ref<FormTracker> form_tracker_;
};

inline FormTrackerTestApi test_api(FormTracker& tracker) {
  return FormTrackerTestApi(&tracker);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_TEST_API_H_
