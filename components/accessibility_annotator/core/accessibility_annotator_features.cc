// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator {

BASE_FEATURE(kContentAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kContentAnnotatorMaxPendingUrls{
    &kContentAnnotator, "content_annotator_max_pending_urls", 10};
const base::FeatureParam<std::string>
    kContentAnnotatorClassifierTitleKeywordRules{
        &kContentAnnotator, "content_annotator_classifier_title_keyword_rules",
        ""};
const base::FeatureParam<std::string> kContentAnnotatorClassifierUrlMatchRules{
    &kContentAnnotator, "content_annotator_classifier_url_match_rules", ""};

}  // namespace accessibility_annotator
