// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/fake_password_parser_service.h"

#include <utility>
#include <vector>

#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"

namespace password_manager {

// A wrapper on CSVPasswordSequence that mimics the sandbox behaviour.
void FakePasswordParserService::ParseCSV(const std::string& raw_json,
                                         ParseCSVCallback callback) {
  mojom::CSVPasswordSequencePtr result = nullptr;
  CSVPasswordSequence seq(raw_json);
  if (seq.result() == CSVPassword::Status::kOK) {
    result = mojom::CSVPasswordSequence::New();
    if (result) {
      for (const auto& pwd : seq) {
        result->csv_passwords.push_back(pwd);
      }
    }
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace password_manager
