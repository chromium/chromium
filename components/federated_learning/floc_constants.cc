// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_constants.h"

namespace federated_learning {

// This is only for experimentation and won't be served to websites.
const uint8_t kMaxNumberOfBitsInFloc = 50;
static_assert(kMaxNumberOfBitsInFloc > 0 &&
                  kMaxNumberOfBitsInFloc <=
                      std::numeric_limits<uint64_t>::digits,
              "Number of bits in the floc id must be greater than 0 and no "
              "greater than 64.");

const char kManifestFlocComponentFormatKey[] = "floc_component_format";

const int kCurrentFlocComponentFormatVersion = 1;

const base::FilePath::CharType kTopLevelDirectoryName[] =
    FILE_PATH_LITERAL("Floc");

const base::FilePath::CharType kBlocklistFileName[] =
    FILE_PATH_LITERAL("Blocklist");

const base::FilePath::CharType kSortingLshClustersFileName[] =
    FILE_PATH_LITERAL("SortingLshClusters");

}  // namespace federated_learning
