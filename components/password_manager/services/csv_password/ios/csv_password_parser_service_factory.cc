// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/ios/csv_password_parser_service_factory.h"

#include "base/memory/singleton.h"
#include "components/password_manager/services/csv_password/csv_password_parser_impl.h"

namespace password_manager {

CSVPasswordParserServiceFactory::CSVPasswordParserServiceFactory() = default;

CSVPasswordParserServiceFactory::~CSVPasswordParserServiceFactory() {
  parser_.reset();
}

CSVPasswordParserServiceFactory*
CSVPasswordParserServiceFactory::GetInstance() {
  return base::Singleton<CSVPasswordParserServiceFactory>::get();
}

mojo::Remote<password_manager::mojom::CSVPasswordParser>
CSVPasswordParserServiceFactory::LaunchCSVPasswordParser() {
  mojo::Remote<password_manager::mojom::CSVPasswordParser> remote;
  mojo::PendingReceiver<password_manager::mojom::CSVPasswordParser> receiver =
      remote.BindNewPipeAndPassReceiver();
  parser_ = std::make_unique<CSVPasswordParserImpl>(std::move(receiver));
  return remote;
}

}  // namespace password_manager
