// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PROTOCOL_SERIALIZER_JSON_H_
#define COMPONENTS_UPDATE_CLIENT_PROTOCOL_SERIALIZER_JSON_H_

#include <string>

#include "components/update_client/protocol_serializer.h"

namespace update_client {

class ProtocolSerializerJSON final : public ProtocolSerializer {
 public:
  ProtocolSerializerJSON() = default;

  ProtocolSerializerJSON(const ProtocolSerializerJSON&) = delete;
  ProtocolSerializerJSON& operator=(const ProtocolSerializerJSON&) = delete;

  // Overrides for ProtocolSerializer.
  std::string Serialize(
      const protocol_request::Request& request) const override;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PROTOCOL_SERIALIZER_JSON_H_
