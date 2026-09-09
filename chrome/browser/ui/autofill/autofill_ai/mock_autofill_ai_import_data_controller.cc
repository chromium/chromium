// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/mock_autofill_ai_import_data_controller.h"

namespace autofill {

MockAutofillAiImportDataController::MockAutofillAiImportDataController() {
  ON_CALL(*this, GetLegalMessageLines())
      .WillByDefault(testing::ReturnRef(legal_message_lines_));
}
MockAutofillAiImportDataController::~MockAutofillAiImportDataController() =
    default;

}  // namespace autofill
