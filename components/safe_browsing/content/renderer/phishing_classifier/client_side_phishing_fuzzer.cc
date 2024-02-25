// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/client_side_phishing_fuzzer.pb.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

DEFINE_PROTO_FUZZER(
    const safe_browsing::ClientSidePhishingFuzzerCase& fuzzing_case) {
  base::CommandLine::Init(0, nullptr);
  const std::string model_str = fuzzing_case.memory_region();
  base::MappedReadOnlyRegion mapped_region = base::MappedReadOnlyRegion();
  mapped_region = base::ReadOnlySharedMemoryRegion::Create(model_str.size());
  memcpy(mapped_region.mapping.memory(), model_str.data(), model_str.length());
  std::unique_ptr<safe_browsing::Scorer> scorer(safe_browsing::Scorer::Create(
      mapped_region.region.Duplicate(), base::File()));
  if (!scorer)
    return;
  safe_browsing::FeatureMap features;
  for (const std::string& boolean_feature : fuzzing_case.boolean_features()) {
    if (!features.AddBooleanFeature(boolean_feature))
      return;
  }

  for (const safe_browsing::ClientSidePhishingFuzzerCase::RealFeature&
           real_feature : fuzzing_case.real_features()) {
    if (!features.AddRealFeature(real_feature.name(), real_feature.value()))
      return;
  }

  scorer->ComputeScore(features);
}
