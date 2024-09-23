// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_registration.h"

#include <stdlib.h>

#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace attribution_reporting {

namespace {

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    base::i18n::InitializeICU();
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

}  // namespace

DEFINE_PROTO_FUZZER(const json_proto::JsonValue& json_value) {
  static Environment env;

  json_proto::JsonProtoConverter converter;
  std::string native_input = converter.Convert(json_value);

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  std::optional<base::Value> input = base::JSONReader::Read(
      native_input, base::JSONParserOptions::JSON_PARSE_RFC);
  if (!input) {
    return;
  }

  // TODO(apaseltiner): Allow `source_type` to be fuzzed.
  std::ignore = SourceRegistration::Parse(*std::move(input),
                                          mojom::SourceType::kNavigation);
}

}  // namespace attribution_reporting
