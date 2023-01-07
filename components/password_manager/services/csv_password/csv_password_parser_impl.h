// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_CSV_PASSWORD_PARSER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_CSV_PASSWORD_PARSER_IMPL_H_

#include <string>

#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace password_manager {

// Implementation of the CSVPasswordParser mojom interface.
class CSVPasswordParserImpl : public mojom::CSVPasswordParser {
 public:
  // Constructs a CSVPasswordParserImpl bound to |receiver|.
  explicit CSVPasswordParserImpl(
      mojo::PendingReceiver<mojom::CSVPasswordParser> receiver);
  ~CSVPasswordParserImpl() override;
  CSVPasswordParserImpl(const CSVPasswordParserImpl&) = delete;
  CSVPasswordParserImpl& operator=(const CSVPasswordParserImpl&) = delete;

  // password_manager::mojom::CSVPasswordParser:
  void ParseCSV(const std::string& raw_json,
                ParseCSVCallback callback) override;

 private:
  mojo::Receiver<mojom::CSVPasswordParser> receiver_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_CSV_PASSWORD_PARSER_IMPL_H_
