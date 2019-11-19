// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_serializer.h"

struct Environment {
  Environment() { CHECK(base::CommandLine::Init(0, nullptr)); }
};

namespace update_client {
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  // Independently, try serializing a Request.
  base::flat_map<std::string, std::string> additional_attributes;
  std::map<std::string, std::string> updater_state_attributes;
  std::vector<protocol_request::App> apps;

  // Share |data| between |MakeProtocolRequest| args
  FuzzedDataProvider data_provider(data, size);
  const size_t max_arg_size = size / 7;
  auto GetUtf8String = [&data_provider, max_arg_size]() -> std::string {
    std::string s = data_provider.ConsumeRandomLengthString(max_arg_size);
    return base::IsStringUTF8(s) ? s : "";
  };

  protocol_request::Request request = MakeProtocolRequest(
      "{" + GetUtf8String() + "}" /* session_id */,
      GetUtf8String() /* prod_id */, GetUtf8String() /* browser_version */,
      GetUtf8String() /* lang */, GetUtf8String() /* channel */,
      GetUtf8String() /* os_long_name */,
      GetUtf8String() /* download_preference */, additional_attributes,
      &updater_state_attributes, std::move(apps));

  update_client::ProtocolHandlerFactoryJSON factory;
  std::unique_ptr<ProtocolSerializer> serializer = factory.CreateSerializer();
  std::string request_serialized = serializer->Serialize(request);

  // Any request we serialize should be valid JSON.
  base::JSONReader json_reader;
  CHECK(json_reader.Read(request_serialized));
  return 0;
}
}  // namespace update_client
