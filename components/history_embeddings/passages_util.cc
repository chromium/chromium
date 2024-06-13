// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/passages_util.h"

#include <string>

#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "third_party/zlib/google/compression_utils.h"

namespace history_embeddings {

std::vector<uint8_t> PassagesProtoToBlob(
    const proto::PassagesValue& passages_value,
    const os_crypt_async::Encryptor& encryptor) {
  // TODO(b/325524013): Add metrics to determine if this needs to be sent to a
  // separate sequence. Whole process for 1.2MB takes about 68ms on a Desktop.
  std::string serialized_proto = passages_value.SerializeAsString();
  if (serialized_proto.empty()) {
    return {};
  }

  // We use zlib here because it's used all over Chromium and has better
  // compression ratios than snappy. We expect this to have a significant impact
  // on user disk space, so the higher compression ratio matters.
  std::string compressed_proto;
  if (!compression::GzipCompress(std::move(serialized_proto),
                                 &compressed_proto)) {
    return {};
  }

  std::string encrypted_compressed_proto;
  if (!encryptor.EncryptString(std::move(compressed_proto),
                               &encrypted_compressed_proto)) {
    return {};
  }

  std::vector<uint8_t> result(encrypted_compressed_proto.begin(),
                              encrypted_compressed_proto.end());
  return result;
}

std::optional<history_embeddings::proto::PassagesValue> PassagesBlobToProto(
    base::span<const uint8_t> passages_blob,
    const os_crypt_async::Encryptor& encryptor) {
  if (passages_blob.empty()) {
    return std::nullopt;
  }

  // TODO(b/325524013): Add metrics to determine if this needs to be sent to a
  // separate sequence.
  std::string passages_blob_as_string(passages_blob.begin(),
                                      passages_blob.end());

  std::string compressed_proto;
  if (!encryptor.DecryptString(passages_blob_as_string, &compressed_proto)) {
    return std::nullopt;
  }

  std::string serialized_proto;
  if (!compression::GzipUncompress(std::move(compressed_proto),
                                   &serialized_proto)) {
    return std::nullopt;
  }

  proto::PassagesValue proto;
  if (!proto.ParseFromString(std::move(serialized_proto))) {
    return std::nullopt;
  }

  return proto;
}

}  // namespace history_embeddings
