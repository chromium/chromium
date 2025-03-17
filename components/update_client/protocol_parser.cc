// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser.h"

#include <string>

#include "base/strings/stringprintf.h"

namespace update_client {

ProtocolParser::ProtocolParser() = default;
ProtocolParser::~ProtocolParser() = default;

ProtocolParser::Results::Results() = default;
ProtocolParser::Results::Results(const Results& other) = default;
ProtocolParser::Results& ProtocolParser::Results::operator=(const Results&) =
    default;
ProtocolParser::Results::~Results() = default;

ProtocolParser::App::App() = default;
ProtocolParser::App::App(const App& other) = default;
ProtocolParser::App& ProtocolParser::App::operator=(const App&) = default;
ProtocolParser::App::~App() = default;

ProtocolParser::Operation::Operation() = default;
ProtocolParser::Operation::Operation(const Operation& other) = default;
ProtocolParser::Operation& ProtocolParser::Operation::operator=(
    const Operation&) = default;
ProtocolParser::Operation::~Operation() = default;

ProtocolParser::Pipeline::Pipeline() = default;
ProtocolParser::Pipeline::Pipeline(const Pipeline& other) = default;
ProtocolParser::Pipeline& ProtocolParser::Pipeline::operator=(const Pipeline&) =
    default;
ProtocolParser::Pipeline::~Pipeline() = default;

ProtocolParser::Data::Data() = default;
ProtocolParser::Data::Data(const Data& other) = default;
ProtocolParser::Data& ProtocolParser::Data::operator=(const Data&) = default;
ProtocolParser::Data::Data(const std::string& install_data_index,
                           const std::string& text)
    : install_data_index(install_data_index), text(text) {}
ProtocolParser::Data::~Data() = default;

void ProtocolParser::ParseError(const char* details, ...) {
  va_list args;
  va_start(args, details);

  if (!errors_.empty()) {
    errors_ += "\r\n";
  }

  base::StringAppendV(&errors_, details, args);
  va_end(args);
}

bool ProtocolParser::Parse(const std::string& response) {
  results_.daystart_elapsed_days = kNoDaystart;
  results_.apps.clear();
  errors_.clear();

  return DoParse(response, &results_);
}

}  // namespace update_client
