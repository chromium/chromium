// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PhishingUrlFeatureExtractor handles computing URL-based features for
// the client-side phishing detection model.  These include tokens in the
// host and path, features pertaining to host length, and IP addresses.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_URL_FEATURE_EXTRACTOR_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_URL_FEATURE_EXTRACTOR_H_

#include <stddef.h>

#include <string>
#include <vector>

class GURL;

namespace safe_browsing {
class FeatureMap;

class PhishingUrlFeatureExtractor {
 public:
  PhishingUrlFeatureExtractor();

  PhishingUrlFeatureExtractor(const PhishingUrlFeatureExtractor&) = delete;
  PhishingUrlFeatureExtractor& operator=(const PhishingUrlFeatureExtractor&) =
      delete;

  ~PhishingUrlFeatureExtractor();

  // Extracts features for |url| into the given feature map.
  // Returns true on success.
  bool ExtractFeatures(const GURL& url, FeatureMap* features);

 private:
  friend class PhishingUrlFeatureExtractorTest;

  static const size_t kMinPathComponentLength = 3;

  // Given a string, finds all substrings of consecutive alphanumeric
  // characters of length >= kMinPathComponentLength and inserts them into
  // tokens.
  static void SplitStringIntoLongAlphanumTokens(
      const std::string& full,
      std::vector<std::string>* tokens);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_URL_FEATURE_EXTRACTOR_H_
