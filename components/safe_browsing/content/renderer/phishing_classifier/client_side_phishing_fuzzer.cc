// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "components/safe_browsing/content/renderer/phishing_classifier/client_side_phishing_fuzzer.pb.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/protobuf_scorer.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

DEFINE_PROTO_FUZZER(
    const safe_browsing::ClientSidePhishingFuzzerCase& fuzzing_case) {
  if (!fuzzing_case.model().IsInitialized())
    return;

  std::string model_str;
  if (!fuzzing_case.model().SerializeToString(&model_str))
    return;
  std::unique_ptr<safe_browsing::Scorer> scorer(
      safe_browsing::ProtobufModelScorer::Create(model_str, base::File()));
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
