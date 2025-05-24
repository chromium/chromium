// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_FAKE_PASSWORD_PARSER_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_FAKE_PASSWORD_PARSER_SERVICE_H_

#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"

namespace password_manager {

// A wrapper on CSVPasswordSequence that mimics the sandbox behaviour.
class FakePasswordParserService : public mojom::CSVPasswordParser {
 public:
  void ParseCSV(const std::string& raw_json,
                ParseCSVCallback callback) override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_FAKE_PASSWORD_PARSER_SERVICE_H_
