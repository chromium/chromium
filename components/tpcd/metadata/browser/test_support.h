// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TPCD_METADATA_BROWSER_TEST_SUPPORT_H_
#define COMPONENTS_TPCD_METADATA_BROWSER_TEST_SUPPORT_H_

#include <cstdint>

#include "components/tpcd/metadata/browser/manager.h"
#include "components/tpcd/metadata/common/proto/metadata.pb.h"

namespace tpcd::metadata {
std::string MakeBase64EncodedMetadata(const Metadata& metadata);
class DeterministicGenerator : public Manager::RandGenerator {
 public:
  explicit DeterministicGenerator(const uint32_t rand = 0) : rand_(rand) {}
  ~DeterministicGenerator() override = default;

  DeterministicGenerator(const DeterministicGenerator&) = delete;
  DeterministicGenerator& operator=(const DeterministicGenerator&) = delete;

  uint32_t Generate() const override;
  // Sets a deterministic number to be returned by the generator.
  void set_rand(uint32_t rand) { rand_ = rand; }

 private:
  uint32_t rand_;
};
}  // namespace tpcd::metadata
#endif  // COMPONENTS_TPCD_METADATA_BROWSER_TEST_SUPPORT_H_
