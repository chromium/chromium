// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_

namespace autofill::autofill_metrics {

// This metric is only relevant for granular filling, i.e. when the delete
// dialog is opened from the Autofill popup.
void LogDeleteAddressProfileDialogClosed(bool user_accepted_delete);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_
