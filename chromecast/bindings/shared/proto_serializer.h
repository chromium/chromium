// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_SHARED_PROTO_SERIALIZER_H_
#define CHROMECAST_BINDINGS_SHARED_PROTO_SERIALIZER_H_

#include <optional>
#include <string_view>

#include "base/base64.h"

namespace chromecast {
namespace bindings {

template <typename T>
class ProtoSerializer {
 public:
  ProtoSerializer() = delete;

  // Serializes |proto| to its base64 representation. Used by bindings
  // frontends and backends for consistent serialization logic.
  static std::string Serialize(T proto) {
    std::vector<uint8_t> encoded(proto.ByteSizeLong());
    if (encoded.empty()) {
      return std::string();
    }

    CHECK(proto.SerializeWithCachedSizesToArray(&encoded[0]));
    std::string ser = base::Base64Encode(encoded);
    return ser;
  }

  // Deserializes |base64| to its proto representation, parsed into |result|.
  // Returns a value if parsing is successful; otherwise, returns false. Used
  // by bindings frontends and backends for consistent serialization logic.
  static std::optional<T> Deserialize(std::string_view base64_proto) {
    std::string decoded;
    if (!base::Base64Decode(base64_proto, &decoded)) {
      return std::nullopt;
    }

    T result;
    return result.ParseFromString(decoded) ? std::make_optional<T>(result)
                                           : std::nullopt;
  }
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_SHARED_PROTO_SERIALIZER_H_
