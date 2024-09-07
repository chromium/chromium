// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/first_cct_page_load_passwords_ukm_recorder.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace password_manager {

FirstCctPageLoadPasswordsUkmRecorder::FirstCctPageLoadPasswordsUkmRecorder(
    ukm::SourceId source_id)
    : ukm_entry_builder_(
          std::make_unique<ukm::builders::PasswordManager_FirstCCTPageLoad>(
              source_id)) {
  ukm_entry_builder_->SetHasPasswordForm(false);
}

FirstCctPageLoadPasswordsUkmRecorder::~FirstCctPageLoadPasswordsUkmRecorder() {
  ukm_entry_builder_->Record(ukm::UkmRecorder::Get());
}

void FirstCctPageLoadPasswordsUkmRecorder::RecordHasPasswordForm() {
  ukm_entry_builder_->SetHasPasswordForm(true);
}

}  // namespace password_manager
