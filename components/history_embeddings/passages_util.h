// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_PASSAGES_UTIL_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_PASSAGES_UTIL_H_

#include <optional>

#include "base/containers/span.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace history_embeddings {

// Compresses and encrypts `passages_value` to a bytestring blob suitable for
// storage in the database. Returns an empty vector on error.
std::vector<uint8_t> PassagesProtoToBlob(
    const proto::PassagesValue& passages_value,
    const os_crypt_async::Encryptor& encryptor);

// Decrypts and then decompresses `passages_blob` suitable for storage in the
// database to the original proto. Note that this can fail for invalid inputs,
// and therefore can return nullopt.
std::optional<proto::PassagesValue> PassagesBlobToProto(
    base::span<const uint8_t> passages_blob,
    const os_crypt_async::Encryptor& encryptor);

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_PASSAGES_UTIL_H_
