// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_handler.h"

#include "components/update_client/protocol_parser_json.h"
#include "components/update_client/protocol_serializer_json.h"

namespace update_client {

std::unique_ptr<ProtocolParser> ProtocolHandlerFactoryJSON::CreateParser()
    const {
  return std::make_unique<ProtocolParserJSON>();
}

std::unique_ptr<ProtocolSerializer>
ProtocolHandlerFactoryJSON::CreateSerializer() const {
  return std::make_unique<ProtocolSerializerJSON>();
}

}  // namespace update_client
