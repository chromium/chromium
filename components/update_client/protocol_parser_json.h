// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_JSON_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_JSON_H_

#include <string>

#include "components/update_client/protocol_parser.h"

namespace update_client {

// Parses responses for the update protocol version 3.
// (https://github.com/google/omaha/blob/wiki/ServerProtocolV3.md)
class ProtocolParserJSON final : public ProtocolParser {
 public:
  ProtocolParserJSON() = default;

  ProtocolParserJSON(const ProtocolParserJSON&) = delete;
  ProtocolParserJSON& operator=(const ProtocolParserJSON&) = delete;

 private:
  // Overrides for ProtocolParser.
  bool DoParse(const std::string& response_json, Results* results) override;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_JSON_H_
