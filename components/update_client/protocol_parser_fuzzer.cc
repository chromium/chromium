// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "components/update_client/protocol_handler.h"

namespace update_client {
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  update_client::ProtocolHandlerFactoryJSON factory;
  std::unique_ptr<ProtocolParser> parser = factory.CreateParser();

  // Try parsing as a Response.
  FuzzedDataProvider data_provider(data, size);
  parser->Parse(data_provider.ConsumeRemainingBytesAsString());

  return 0;
}
}  // namespace update_client
