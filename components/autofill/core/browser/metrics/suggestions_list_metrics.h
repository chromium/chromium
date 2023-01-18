// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_

namespace autofill {
enum class PopupType;

namespace autofill_metrics {

// Log the index of the selected Autofill suggestion in the popup.
void LogAutofillSuggestionAcceptedIndex(int index,
                                        autofill::PopupType popup_type,
                                        bool off_the_record);

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
