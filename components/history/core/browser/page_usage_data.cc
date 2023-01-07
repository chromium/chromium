// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/page_usage_data.h"

namespace history {

PageUsageData::PageUsageData(SegmentID id) : id_(id), score_(0.0) {
}

PageUsageData::~PageUsageData() {
}

}  // namespace history
