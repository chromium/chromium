// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/form_parsing/fuzzer/data_accessor.h"
#include "components/password_manager/core/browser/form_parsing/fuzzer/form_data_producer.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

// ICU is used inside GURL parser, which is used by GenerateWithDataAccessor.
struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  DataAccessor accessor(data, size);
  FormDataParser::Mode mode = accessor.ConsumeBit()
                                  ? FormDataParser::Mode::kFilling
                                  : FormDataParser::Mode::kSaving;

  bool use_predictions = accessor.ConsumeBit();
  FormPredictions predictions;
  autofill::FormData form_data = GenerateWithDataAccessor(
      &accessor, use_predictions ? &predictions : nullptr);

  FormDataParser parser;
  if (use_predictions)
    parser.set_predictions(predictions);

  std::unique_ptr<PasswordForm> result = parser.Parse(form_data, mode);
  if (result) {
    // Create a copy of the result -- running the copy-constructor might
    // discover some invalid data in |result|.
    PasswordForm copy(*result);
  }
  return 0;
}

}  // namespace password_manager
