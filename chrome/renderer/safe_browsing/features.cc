// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/features.h"

#include "base/metrics/histogram_macros.h"

namespace safe_browsing {

const size_t FeatureMap::kMaxFeatureMapSize = 10000;

FeatureMap::FeatureMap() {}
FeatureMap::~FeatureMap() {}

bool FeatureMap::AddBooleanFeature(const std::string& name) {
  return AddRealFeature(name, 1.0);
}

bool FeatureMap::AddRealFeature(const std::string& name, double value) {
  if (features_.size() >= kMaxFeatureMapSize) {
    // If we hit this case, it indicates that either kMaxFeatureMapSize is
    // too small, or there is a bug causing too many features to be added.
    // In this case, we'll log to a histogram so we can see that this is
    // happening, and make phishing classification fail silently.
    UMA_HISTOGRAM_COUNTS_1M("SBClientPhishing.TooManyFeatures", 1);
    return false;
  }
  // We only expect features in the range [0.0, 1.0], so fail if the feature is
  // outside this range.
  if (value < 0.0 || value > 1.0) {
    UMA_HISTOGRAM_COUNTS_1M("SBClientPhishing.IllegalFeatureValue", 1);
    return false;
  }

  features_[name] = value;
  return true;
}

void FeatureMap::Clear() {
  features_.clear();
}

namespace features {
// URL host features
const char kUrlHostIsIpAddress[] = "UrlHostIsIpAddress";
const char kUrlTldToken[] = "UrlTld=";
const char kUrlDomainToken[] = "UrlDomain=";
const char kUrlOtherHostToken[] = "UrlOtherHostToken=";

// URL host aggregate features
const char kUrlNumOtherHostTokensGTOne[] = "UrlNumOtherHostTokens>1";
const char kUrlNumOtherHostTokensGTThree[] = "UrlNumOtherHostTokens>3";

// URL path features
const char kUrlPathToken[] = "UrlPathToken=";

// DOM HTML form features
const char kPageHasForms[] = "PageHasForms";
const char kPageActionOtherDomainFreq[] = "PageActionOtherDomainFreq";
const char kPageActionURL[] = "PageActionURL=";
const char kPageHasTextInputs[] = "PageHasTextInputs";
const char kPageHasPswdInputs[] = "PageHasPswdInputs";
const char kPageHasRadioInputs[] = "PageHasRadioInputs";
const char kPageHasCheckInputs[] = "PageHasCheckInputs";

// DOM HTML link features
const char kPageExternalLinksFreq[] = "PageExternalLinksFreq";
const char kPageLinkDomain[] = "PageLinkDomain=";
const char kPageSecureLinksFreq[] = "PageSecureLinksFreq";

// DOM HTML script features
const char kPageNumScriptTagsGTOne[] = "PageNumScriptTags>1";
const char kPageNumScriptTagsGTSix[] = "PageNumScriptTags>6";

// Other DOM HTML features
const char kPageImgOtherDomainFreq[] = "PageImgOtherDomainFreq";

// Page term features
const char kPageTerm[] = "PageTerm=";

}  // namespace features
}  // namespace safe_browsing
