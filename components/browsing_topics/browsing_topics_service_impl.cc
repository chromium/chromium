// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_service_impl.h"

namespace browsing_topics {

BrowsingTopicsServiceImpl::~BrowsingTopicsServiceImpl() = default;

BrowsingTopicsServiceImpl::BrowsingTopicsServiceImpl() = default;

std::vector<privacy_sandbox::CanonicalTopic>
BrowsingTopicsServiceImpl::GetTopicsForSiteForDisplay(
    const url::Origin& top_origin) const {
  return {};
}

std::vector<privacy_sandbox::CanonicalTopic>
BrowsingTopicsServiceImpl::GetTopTopicsForDisplay() const {
  return {};
}

}  // namespace browsing_topics
