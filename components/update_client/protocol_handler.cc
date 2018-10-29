// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_parser_xml.h"
#include "components/update_client/protocol_serializer_xml.h"

namespace update_client {

std::unique_ptr<ProtocolParser> ProtocolHandlerFactoryXml::CreateParser()
    const {
  return std::make_unique<ProtocolParserXml>();
}

std::unique_ptr<ProtocolSerializer>
ProtocolHandlerFactoryXml::CreateSerializer() const {
  return std::make_unique<ProtocolSerializerXml>();
}

}  // namespace update_client
