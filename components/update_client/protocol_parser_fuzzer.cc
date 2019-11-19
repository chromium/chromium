// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_parser.h"

namespace update_client {
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  update_client::ProtocolHandlerFactoryJSON factory;
  std::unique_ptr<ProtocolParser> parser = factory.CreateParser();

  // Try parsing as a Response.
  const std::string response(reinterpret_cast<const char*>(data), size);
  parser->Parse(response);

  return 0;
}
}  // namespace update_client
