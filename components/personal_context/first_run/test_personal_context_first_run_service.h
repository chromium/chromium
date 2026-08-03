// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_TEST_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_TEST_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_

#include <optional>

#include "components/personal_context/first_run/personal_context_first_run_service.h"

namespace personal_context {

class TestPersonalContextFirstRunService
    : public PersonalContextFirstRunService {
 public:
  TestPersonalContextFirstRunService();
  TestPersonalContextFirstRunService(
      const TestPersonalContextFirstRunService&) = delete;
  TestPersonalContextFirstRunService& operator=(
      const TestPersonalContextFirstRunService&) = delete;
  ~TestPersonalContextFirstRunService() override;

  // PersonalContextFirstRunService:
  void MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() override;
  bool ShouldShowPersonalContextAmbientAutofillNotice() const override;
  void RecordAmbientAutofillNoticeImpression(uint32_t session_id) override;
  void MarkPersonalContextInAtMemoryNoticeAsAcknowledged() override;
  bool ShouldShowPersonalContextAtMemoryNotice() const override;
  void RecordAtMemoryNoticeImpression(uint32_t session_id) override;

  void set_should_show_ambient_autofill_notice(bool should_show) {
    should_show_ambient_autofill_notice_ = should_show;
  }
  bool is_ambient_autofill_notice_acknowledged() const {
    return is_ambient_autofill_notice_acknowledged_;
  }
  int ambient_autofill_notice_impressions() const {
    return ambient_autofill_notice_impressions_;
  }

  void set_should_show_at_memory_notice(bool should_show) {
    should_show_at_memory_notice_ = should_show;
  }
  bool is_at_memory_notice_acknowledged() const {
    return is_at_memory_notice_acknowledged_;
  }
  int at_memory_notice_impressions() const {
    return at_memory_notice_impressions_;
  }

 private:
  bool should_show_ambient_autofill_notice_ = false;
  bool is_ambient_autofill_notice_acknowledged_ = false;
  int ambient_autofill_notice_impressions_ = 0;
  std::optional<uint32_t> last_ambient_autofill_session_id_;

  bool should_show_at_memory_notice_ = false;
  bool is_at_memory_notice_acknowledged_ = false;
  int at_memory_notice_impressions_ = 0;
  std::optional<uint32_t> last_at_memory_session_id_;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_TEST_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_H_
