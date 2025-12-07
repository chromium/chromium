// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/csv_password_parser_service.h"

#include "components/password_manager/services/csv_password/ios/csv_password_parser_service_factory.h"

namespace password_manager {

mojo::Remote<password_manager::mojom::CSVPasswordParser>
LaunchCSVPasswordParser() {
  return password_manager::CSVPasswordParserServiceFactory::GetInstance()
      ->LaunchCSVPasswordParser();
}

}  // namespace password_manager
