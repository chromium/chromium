// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/csv_password_parser_impl.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"

namespace password_manager {

CSVPasswordParserImpl::CSVPasswordParserImpl(
    mojo::PendingReceiver<mojom::CSVPasswordParser> receiver)
    : receiver_(this, std::move(receiver)) {}

CSVPasswordParserImpl::~CSVPasswordParserImpl() = default;

void CSVPasswordParserImpl::ParseCSV(const std::string& raw_csv,
                                     ParseCSVCallback callback) {
  mojom::CSVPasswordSequencePtr result;
  if (auto seq = CSVPasswordSequence::Create(raw_csv); seq.has_value()) {
    result = mojom::CSVPasswordSequence::New();
    while (std::optional<CSVPassword> pwd = seq->NextPassword()) {
      result->csv_passwords.push_back(*std::move(pwd));
    }
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace password_manager
