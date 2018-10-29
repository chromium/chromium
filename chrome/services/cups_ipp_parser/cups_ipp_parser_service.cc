// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_parser/cups_ipp_parser_service.h"

#include "base/logging.h"
#include "chrome/services/cups_ipp_parser/ipp_parser.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

void OnIppParserRequest(service_manager::ServiceContextRefFactory* ref_factory,
                        chrome::mojom::IppParserRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<chrome::IppParser>(ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

CupsIppParserService::CupsIppParserService() = default;

CupsIppParserService::~CupsIppParserService() = default;

std::unique_ptr<service_manager::Service>
CupsIppParserService::CreateService() {
  return std::make_unique<CupsIppParserService>();
}

void CupsIppParserService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      context()->CreateQuitClosure());
  registry_.AddInterface(
      base::BindRepeating(&OnIppParserRequest, ref_factory_.get()));

  DVLOG(1) << "CupsIppParserService started.";
}

void CupsIppParserService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
