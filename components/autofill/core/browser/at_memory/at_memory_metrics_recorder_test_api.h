// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

class AtMemoryMetricsRecorderTestApi {
 public:
  explicit AtMemoryMetricsRecorderTestApi(AtMemoryMetricsRecorder* recorder)
      : recorder_(CHECK_DEREF(recorder)) {}

  FormSignature form_signature() const { return recorder_->form_signature_; }

  FieldSignature field_signature() const { return recorder_->field_signature_; }

 private:
  raw_ref<AtMemoryMetricsRecorder> recorder_;
};

inline AtMemoryMetricsRecorderTestApi test_api(
    AtMemoryMetricsRecorder& recorder) {
  return AtMemoryMetricsRecorderTestApi(&recorder);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_TEST_API_H_
