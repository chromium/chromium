// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_IOS_SHARED_EXPERIMENTS_TRANSLATOR_H_
#define COMPONENTS_FEED_CORE_V2_IOS_SHARED_EXPERIMENTS_TRANSLATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "components/feed/core/proto/v2/wire/chrome_feed_response_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feed {

// A map of trial names (key) and list of group names/IDs (value)
// sent from the server.
typedef std::map<std::string, std::vector<std::string>> Experiments;

constexpr const char kDiscoverFeedExperiments[] = "DiscoverFeedExperiments";

absl::optional<Experiments> TranslateExperiments(
    const google::protobuf::RepeatedPtrField<feedwire::Experiment>&
        wire_experiments);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_IOS_SHARED_EXPERIMENTS_TRANSLATOR_H_
