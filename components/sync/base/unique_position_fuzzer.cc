// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/unique_position.h"

#include <string>
#include <string_view>

#include "components/sync/protocol/unique_position.pb.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view data_piece(reinterpret_cast<const char*>(data), size);

  sync_pb::UniquePosition unique_position_proto;
  unique_position_proto.set_custom_compressed_v1(data_piece);
  syncer::UniquePosition::FromProto(unique_position_proto);
  return 0;
}
