// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_XML_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_XML_H_

#include <string>

#include "base/macros.h"
#include "components/update_client/protocol_parser.h"

namespace update_client {

// Parses responses for the update protocol version 3.
// (https://github.com/google/omaha/blob/wiki/ServerProtocolV3.md)
class ProtocolParserXml final : public ProtocolParser {
 public:
  ProtocolParserXml() = default;

 private:
  // Overrides for ProtocolParser.
  bool DoParse(const std::string& response_xml, Results* results) override;

  DISALLOW_COPY_AND_ASSIGN(ProtocolParserXml);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_PARSER_XML_H_
