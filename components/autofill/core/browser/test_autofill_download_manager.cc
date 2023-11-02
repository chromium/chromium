// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_download_manager.h"

#include "components/autofill/core/browser/form_structure.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestAutofillDownloadManager::TestAutofillDownloadManager(
    AutofillDriver* driver,
    AutofillDownloadManager::Observer* observer)
    : AutofillDownloadManager(driver, observer) {}

TestAutofillDownloadManager::~TestAutofillDownloadManager() {}

bool TestAutofillDownloadManager::StartQueryRequest(
    const std::vector<FormStructure*>& forms) {
  last_queried_forms_ = forms;
  return true;
}

void TestAutofillDownloadManager::VerifyLastQueriedForms(
    const std::vector<FormData>& expected_forms) {
  ASSERT_EQ(expected_forms.size(), last_queried_forms_.size());
  for (size_t i = 0; i < expected_forms.size(); ++i) {
    EXPECT_EQ(last_queried_forms_[i]->global_id().renderer_id,
              expected_forms[i].global_id().renderer_id);
  }
}

}  // namespace autofill
