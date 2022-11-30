// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_TEST_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_TEST_UTIL_H_

#include <memory>

#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/gurl.h"

namespace optimization_guide {

enum class HintsFetcherRemoteResponseType {
  kSuccessful = 0,
  kUnsuccessful = 1,
  kMalformed = 2,
  kHung = 3,
};

// File paths that can be used in testing, handling platform differences, namely
// C:\ in Windows.
extern const char kTestAbsoluteFilePath[];
extern const char kTestRelativeFilePath[];

// Creates the hints config with |optimization_type| to |hints_url| that returns
// the |metadata|. This config string can be passed to the |kHintsProtoOverride|
// commandline switch.
std::string CreateHintsConfig(const GURL& hints_url,
                              proto::OptimizationType optimization_type,
                              proto::Any* metadata);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_TEST_UTIL_H_
