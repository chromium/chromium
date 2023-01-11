// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/compression/test_compression_module.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::Invoke;

namespace reporting {
namespace test {

constexpr size_t kCompressionThreshold = 2;
const CompressionInformation::CompressionAlgorithm kCompressionType =
    CompressionInformation::COMPRESSION_NONE;

TestCompressionModuleStrict::TestCompressionModuleStrict()
    : CompressionModule(kCompressionThreshold, kCompressionType) {
  ON_CALL(*this, CompressRecord)
      .WillByDefault(Invoke(
          [](std::string record,
             scoped_refptr<ResourceManager> resource_manager,
             base::OnceCallback<void(
                 std::string, absl::optional<CompressionInformation>)> cb) {
            // compression_info is not set.
            std::move(cb).Run(record, absl::nullopt);
          }));
}

TestCompressionModuleStrict::~TestCompressionModuleStrict() = default;

}  // namespace test
}  // namespace reporting
