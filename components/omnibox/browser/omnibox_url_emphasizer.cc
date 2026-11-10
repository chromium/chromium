// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/jni_string.h"  // IWYU pragma: keep
#include "base/check.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier_android.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/omnibox/browser/scheme_classifier_jni/OmniboxUrlEmphasizer_jni.h"

// static
static std::vector<int32_t>
JNI_OmniboxUrlEmphasizer_ParseForEmphasizeComponents(
    const std::u16string& text,
    AutocompleteSchemeClassifier* autocomplete_scheme_classifier) {
  CHECK(autocomplete_scheme_classifier);

  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      text, *autocomplete_scheme_classifier, &scheme, &host);

  return {scheme.begin, scheme.len, host.begin, host.len};
}

DEFINE_JNI(OmniboxUrlEmphasizer)
