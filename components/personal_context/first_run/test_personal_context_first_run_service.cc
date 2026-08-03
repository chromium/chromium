// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/first_run/test_personal_context_first_run_service.h"

namespace personal_context {

TestPersonalContextFirstRunService::TestPersonalContextFirstRunService() =
    default;

TestPersonalContextFirstRunService::~TestPersonalContextFirstRunService() =
    default;

void TestPersonalContextFirstRunService::
    MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() {
  is_ambient_autofill_notice_acknowledged_ = true;
}

bool TestPersonalContextFirstRunService::
    ShouldShowPersonalContextAmbientAutofillNotice() const {
  return should_show_ambient_autofill_notice_;
}

void TestPersonalContextFirstRunService::RecordAmbientAutofillNoticeImpression(
    uint32_t session_id) {
  if (!last_ambient_autofill_session_id_ ||
      session_id != *last_ambient_autofill_session_id_) {
    ambient_autofill_notice_impressions_++;
    last_ambient_autofill_session_id_ = session_id;
  }
}

void TestPersonalContextFirstRunService::
    MarkPersonalContextInAtMemoryNoticeAsAcknowledged() {
  is_at_memory_notice_acknowledged_ = true;
  is_ambient_autofill_notice_acknowledged_ = true;
}

bool TestPersonalContextFirstRunService::
    ShouldShowPersonalContextAtMemoryNotice() const {
  return should_show_at_memory_notice_;
}

void TestPersonalContextFirstRunService::RecordAtMemoryNoticeImpression(
    uint32_t session_id) {
  if (!last_at_memory_session_id_ ||
      session_id != *last_at_memory_session_id_) {
    at_memory_notice_impressions_++;
    last_at_memory_session_id_ = session_id;
  }
}

}  // namespace personal_context
