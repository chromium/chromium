// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

namespace {

bool IsValid(CSVPassword::Status status) {
  switch (status) {
    case CSVPassword::Status::kOK:
    case CSVPassword::Status::kSyntaxError:
    case CSVPassword::Status::kSemanticError:
      return true;
  }
  return false;
}

}  // namespace

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CSVPasswordSequence seq(
      std::string(reinterpret_cast<const char*>(data), size));
  CHECK(IsValid(seq.result()))
      << "Invalid parsing result of the whole sequence: "
      << static_cast<int>(seq.result());
  PasswordForm form, copy;
  for (const auto& pwd : seq) {
    const CSVPassword::Status status = pwd.Parse(&form);
    CHECK(IsValid(status)) << "Invalid parsing result of one row: "
                           << static_cast<int>(status);
    if (status == CSVPassword::Status::kOK) {
      // Copy the parsed password to access all its data members and allow the
      // ASAN to detect any corrupted memory inside.
      copy = form;
    }
  }
  return 0;
}

}  // namespace password_manager
