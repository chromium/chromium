// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/form_parsing/fuzzer/form_data_essentials.pb.h"
#include "components/password_manager/core/browser/form_parsing/fuzzer/form_data_proto_producer.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace password_manager {

// ICU is used inside GURL parser, which is used by GenerateWithDataAccessor.
struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

DEFINE_BINARY_PROTO_FUZZER(const ::form_data_fuzzer::Form& form_proto) {
  FormDataParser::Mode mode = form_proto.is_mode_filling()
                                  ? FormDataParser::Mode::kFilling
                                  : FormDataParser::Mode::kSaving;
  autofill::FormData form_data = GenerateWithProto(form_proto);

  FormDataParser parser;
  std::unique_ptr<autofill::PasswordForm> result =
      parser.Parse(form_data, mode);
  if (result) {
    // Create a copy of the result -- running the copy-constructor might
    // discover some invalid data in |result|.
    autofill::PasswordForm copy(*result);
  }
}

}  // namespace password_manager
