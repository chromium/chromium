// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser.h"

#include <string>
#include <string_view>

namespace update_client {

ProtocolParser::ProtocolParser() = default;
ProtocolParser::~ProtocolParser() = default;

void ProtocolParser::ParseError(std::string_view details) {
  if (!errors_.empty()) {
    errors_ += "\r\n";
  }
  errors_.append(details);
}

bool ProtocolParser::Parse(const std::string& response) {
  results_.daystart_elapsed_days = kNoDaystart;
  results_.apps.clear();
  errors_.clear();

  return DoParse(response, &results_);
}

}  // namespace update_client
