// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_SCHEME_CLASSIFIER_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_SCHEME_CLASSIFIER_H_

#include <string>

#include "components/omnibox/browser/autocomplete_scheme_classifier.h"

// The subclass of AutocompleteSchemeClassifier for testing.
class TestSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  TestSchemeClassifier();
  ~TestSchemeClassifier() override;
  TestSchemeClassifier(const TestSchemeClassifier&) = delete;
  TestSchemeClassifier& operator=(const TestSchemeClassifier&) = delete;

  // Overridden from AutocompleteInputSchemeChecker:
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_SCHEME_CLASSIFIER_H_
