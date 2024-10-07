// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIRST_CCT_PAGE_LOAD_PASSWORDS_UKM_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIRST_CCT_PAGE_LOAD_PASSWORDS_UKM_RECORDER_H_

#include <memory>

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm {
namespace builders {
class PasswordManager_FirstCCTPageLoad;
}
}  // namespace ukm

namespace password_manager {

// Reports password manager UKM for CCTs. The metrics are cached during its
// lifetime and reported on destruction. Created only for custom tabs
// on Android.
class FirstCctPageLoadPasswordsUkmRecorder {
 public:
  // Records UKM metrics and reports them on destruction.
  explicit FirstCctPageLoadPasswordsUkmRecorder(ukm::SourceId source_id);
  FirstCctPageLoadPasswordsUkmRecorder(
      const FirstCctPageLoadPasswordsUkmRecorder&) = delete;
  FirstCctPageLoadPasswordsUkmRecorder& operator=(
      const FirstCctPageLoadPasswordsUkmRecorder&) = delete;

  ~FirstCctPageLoadPasswordsUkmRecorder();

  // Records that the page has a password form. Only used for the first page
  // load of a CCT on Android.
  void RecordHasPasswordForm();

 private:
  // Records URL keyed metrics (UKMs) for the first CCT page load upon its
  // destruction. Not constructed on subsequent page loads.
  std::unique_ptr<ukm::builders::PasswordManager_FirstCCTPageLoad>
      ukm_entry_builder_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIRST_CCT_PAGE_LOAD_PASSWORDS_UKM_RECORDER_H_
