// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/fake_password_parser_service.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"

namespace password_manager {

// A wrapper on CSVPasswordSequence that mimics the sandbox behaviour.
void FakePasswordParserService::ParseCSV(const std::string& raw_json,
                                         ParseCSVCallback callback) {
  mojom::CSVPasswordSequencePtr result;
  if (auto seq = CSVPasswordSequence::Create(raw_json); seq.has_value()) {
    result = mojom::CSVPasswordSequence::New();
    while (std::optional<CSVPassword> pwd = seq->NextPassword()) {
      result->csv_passwords.push_back(*std::move(pwd));
    }
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace password_manager
