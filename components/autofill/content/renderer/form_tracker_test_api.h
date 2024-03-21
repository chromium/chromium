// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/content/renderer/form_tracker.h"

namespace autofill {

class FormTrackerTestApi {
 public:
  explicit FormTrackerTestApi(FormTracker* agent) : form_tracker_(*agent) {}

  void DidFinishSameDocumentNavigation() {
    return form_tracker_->DidFinishSameDocumentNavigation();
  }

  void FireProbablyFormSubmitted() {
    form_tracker_->FireProbablyFormSubmitted();
  }

 private:
  const raw_ref<FormTracker> form_tracker_;
};

inline FormTrackerTestApi test_api(FormTracker& tracker) {
  return FormTrackerTestApi(&tracker);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FORM_TRACKER_TEST_API_H_
