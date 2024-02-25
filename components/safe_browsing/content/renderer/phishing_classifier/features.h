// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common types and constants for extracting and evaluating features in the
// client-side phishing detection model.  A feature is simply a string and an
// associated floating-point value between 0 and 1.  The phishing
// classification model contains rules which give an appropriate weight to each
// feature or combination of features.  These values can then be summed to
// compute a final phishiness score.
//
// Some features are boolean features.  If these features are set, they always
// have a value of 0.0 or 1.0.  In practice, the features are only set if the
// value is true (1.0).
//
// We also use token features.  These features have a unique name that is
// constructed from the URL or page contents that we are classifying, for
// example, "UrlDomain=chromium".  These features are also always set to 1.0
// if they are present.
//
// The intermediate storage of the features for a URL is a FeatureMap, which is
// just a thin wrapper around a map of feature name to value.  The entire set
// of features for a URL is extracted before we do any scoring.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_FEATURES_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_FEATURES_H_

#include <stddef.h>
#include <string>
#include <unordered_map>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace safe_browsing {

// Container for a map of features to values, which enforces behavior
// such as a maximum number of features in the map.
class FeatureMap {
 public:
  FeatureMap();

  FeatureMap(const FeatureMap&) = delete;
  FeatureMap& operator=(const FeatureMap&) = delete;

  ~FeatureMap();

  // Adds a boolean feature to a FeatureMap with a value of 1.0.
  // Returns true on success, or false if the feature map exceeds
  // kMaxFeatureMapSize.
  bool AddBooleanFeature(const std::string& name);

  // Adds a real-valued feature to a FeatureMap with the given value.
  // Values must always be in the range [0.0, 1.0].  Returns true on
  // success, or false if the feature map exceeds kMaxFeatureMapSize
  // or the value is outside of the allowed range.
  bool AddRealFeature(const std::string& name, double value);

  // Provides read-only access to the current set of features.
  const std::unordered_map<std::string, double>& features() const {
    return features_;
  }

  // Clears the set of features in the map.
  void Clear();

  // This is an upper bound on the number of features that will be extracted.
  // We should never hit this cap; it is intended as a sanity check to prevent
  // the FeatureMap from growing too large.
  static const size_t kMaxFeatureMapSize;

 private:
  std::unordered_map<std::string, double> features_;
};

BASE_DECLARE_FEATURE(kClientSideDetectionRetryLimit);

extern const base::FeatureParam<int> kClientSideDetectionRetryLimitTime;

namespace features {
// Constants for the various feature names that we use.
//
// IMPORTANT: when adding new features, you must update kAllowedFeatures in
// chrome/browser/safe_browsing/client_side_detection_service.cc if the feature
// should be sent in sanitized pingbacks.

////////////////////////////////////////////////////
// URL host features
////////////////////////////////////////////////////

// Set if the URL's hostname is an IP address.
extern const char kUrlHostIsIpAddress[];
// Token feature containing the portion of the hostname controlled by a
// registrar, for example "com" or "co.uk".
extern const char kUrlTldToken[];
// Token feature containing the first host component below the registrar.
// For example, in "www.google.com", the domain would be "google".
extern const char kUrlDomainToken[];
// Token feature containing each host component below the domain.
// For example, in "www.host.example.com", both "www" and "host" would be
// "other host tokens".
extern const char kUrlOtherHostToken[];

////////////////////////////////////////////////////
// Aggregate features for URL host tokens
////////////////////////////////////////////////////

// Set if the number of "other" host tokens for a URL is greater than one.
// Longer hostnames, regardless of the specific tokens, can be a signal that
// the URL is phishy.
extern const char kUrlNumOtherHostTokensGTOne[];
// Set if the number of "other" host tokens for a URL is greater than three.
extern const char kUrlNumOtherHostTokensGTThree[];

////////////////////////////////////////////////////
// URL path token features
////////////////////////////////////////////////////

// Token feature containing each alphanumeric string in the path that is at
// least 3 characters long.  For example, "/abc/d/efg" would have 2 path
// token features, "abc" and "efg".  Query parameters are not included.
extern const char kUrlPathToken[];

////////////////////////////////////////////////////
// DOM HTML form features
////////////////////////////////////////////////////

// Set if the page has any <form> elements.
extern const char kPageHasForms[];
// The fraction of form elements whose |action| attribute points to a
// URL on a different domain from the document URL.
extern const char kPageActionOtherDomainFreq[];
// Token feature containing each URL that an |action| attribute
// points to.
extern const char kPageActionURL[];
// Set if the page has any <input type="text"> elements
// (includes inputs with missing or unknown types).
extern const char kPageHasTextInputs[];
// Set if the page has any <input type="password"> elements.
extern const char kPageHasPswdInputs[];
// Set if the page has any <input type="radio"> elements.
extern const char kPageHasRadioInputs[];
// Set if the page has any <input type="checkbox"> elements.
extern const char kPageHasCheckInputs[];

////////////////////////////////////////////////////
// DOM HTML link features
////////////////////////////////////////////////////

// The fraction of links in the page which point to a domain other than the
// domain of the document.  See "URL host features" above for a discussion
// of how the doamin is computed.
extern const char kPageExternalLinksFreq[];
// Token feature containing each external domain that is linked to.
extern const char kPageLinkDomain[];
// Fraction of links in the page that use https.
extern const char kPageSecureLinksFreq[];

////////////////////////////////////////////////////
// DOM HTML script features
////////////////////////////////////////////////////

// Set if the number of <script> elements in the page is greater than 1.
extern const char kPageNumScriptTagsGTOne[];
// Set if the number of <script> elements in the page is greater than 6.
extern const char kPageNumScriptTagsGTSix[];

////////////////////////////////////////////////////
// Other DOM HTML features
////////////////////////////////////////////////////

// The fraction of images whose src attribute points to an external domain.
extern const char kPageImgOtherDomainFreq[];

////////////////////////////////////////////////////
// Page term features
////////////////////////////////////////////////////

// Token feature for a term (whitespace-delimited) on a page.  Terms can be
// single words or multi-word n-grams.  Rather than adding this feature for
// every possible token on a page, only the terms that are mentioned in the
// classification model are added.
extern const char kPageTerm[];

}  // namespace features
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_FEATURES_H_
