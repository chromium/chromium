// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/test_previews_decider.h"

namespace previews {

TestPreviewsDecider::TestPreviewsDecider(bool allow_previews)
    : allow_previews_(allow_previews) {}

TestPreviewsDecider::~TestPreviewsDecider() {}

bool TestPreviewsDecider::ShouldAllowPreviewAtECT(
    PreviewsUserData* previews_data,
    const GURL& url,
    bool is_reload,
    PreviewsType type,
    net::EffectiveConnectionType effective_connection_type_threshold,
    const std::vector<std::string>& host_blacklist_from_finch,
    bool is_server_preview) const {
  return allow_previews_;
}

bool TestPreviewsDecider::ShouldAllowPreview(PreviewsUserData* previews_data,
                                             const GURL& url,
                                             bool is_reload,
                                             PreviewsType type) const {
  return allow_previews_;
}

bool TestPreviewsDecider::IsURLAllowedForPreview(
    PreviewsUserData* previews_data,
    const GURL& url,
    PreviewsType type) const {
  return allow_previews_;
}

void TestPreviewsDecider::LoadResourceHints(const GURL& url) {}

void TestPreviewsDecider::LogHintCacheMatch(const GURL& url,
                                            bool is_committed) const {}

}  // namespace previews
