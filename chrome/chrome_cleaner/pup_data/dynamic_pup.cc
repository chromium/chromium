// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/dynamic_pup.h"

namespace chrome_cleaner {

DynamicPUP::DynamicPUP(const std::string& name, UwSId id, PUPData::Flags flags)
    : PUPData::PUP(&stored_signature_),
      stored_name_(name),
      stored_signature_{id,
                        flags,
                        stored_name_.c_str(),
                        /*max_files_to_remove=*/0,
                        kNoDisk,
                        kNoRegistry,
                        kNoCustomMatcher} {}

}  // namespace chrome_cleaner
