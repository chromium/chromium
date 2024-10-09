// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_

#include <string>

#include "third_party/metrics_proto/omnibox_input_type.pb.h"

// An interface that gives embedders the ability to automatically classify the
// omnibox input type based on an explicitly-specified schemes.  If users type
// an input with an explicit scheme other than http, https, or file, this class
// will be used to try and determine whether the input should be treated as a
// URL (for known schemes we want to handle) or a query (for known schemes that
// should be blocked), or if the scheme alone isn't sufficient to make a
// determination.
class AutocompleteSchemeClassifier {
 public:
  virtual ~AutocompleteSchemeClassifier() = default;

  // Checks |scheme| and returns the type of the input if the scheme is known
  // and not blocked. Returns metrics::OmniboxInputType::EMPTY if it's unknown
  // or the classifier implementation cannot handle.
  virtual metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_
