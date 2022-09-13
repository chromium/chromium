// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/csv_password_parser_impl.h"

#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"

#include <utility>
#include <vector>

namespace password_manager {

CSVPasswordParserImpl::CSVPasswordParserImpl(
    mojo::PendingReceiver<mojom::CSVPasswordParser> receiver)
    : receiver_(this, std::move(receiver)) {}

CSVPasswordParserImpl::~CSVPasswordParserImpl() = default;

void CSVPasswordParserImpl::ParseCSV(const std::string& raw_json,
                                     ParseCSVCallback callback) {
  mojom::CSVPasswordSequencePtr result = nullptr;
  CSVPasswordSequence seq(raw_json);
  if (seq.result() == CSVPassword::Status::kOK) {
    result = mojom::CSVPasswordSequence::New();
    for (const auto& pwd : seq)
      result->csv_passwords.push_back(pwd);
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace password_manager
