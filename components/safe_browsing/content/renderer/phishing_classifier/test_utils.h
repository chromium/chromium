// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_TEST_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_TEST_UTILS_H_

namespace safe_browsing {
class FeatureMap;

// Compares two FeatureMap objects using gMock.  Always use this instead of
// operator== or ContainerEq, since hash_map's equality operator may return
// false if the elements were inserted in different orders.
void ExpectFeatureMapsAreEqual(const FeatureMap& first,
                               const FeatureMap& second);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_TEST_UTILS_H_
