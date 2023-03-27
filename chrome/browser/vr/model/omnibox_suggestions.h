// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_
#define CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_

#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "url/gurl.h"

namespace vr {

struct VR_BASE_EXPORT Autocompletion {
  Autocompletion();
  Autocompletion(const std::u16string& new_input,
                 const std::u16string& new_suffix);

  bool operator==(const Autocompletion& other) const;
  bool operator!=(const Autocompletion& other) const {
    return !(*this == other);
  }

  // Input string that yielded the autocomplete text.
  std::u16string input;
  // The suffix to be appended to |input| to generate a complete match.
  std::u16string suffix;
};

struct VR_BASE_EXPORT OmniboxSuggestion {
  OmniboxSuggestion();

  OmniboxSuggestion(const std::u16string& new_contents,
                    const std::u16string& new_description,
                    const AutocompleteMatch::ACMatchClassifications&
                        new_contents_classifications,
                    const AutocompleteMatch::ACMatchClassifications&
                        new_description_classifications,
                    const gfx::VectorIcon* icon,
                    GURL new_destination,
                    const std::u16string& new_input,
                    const std::u16string& new_inline_autocompletion);
  OmniboxSuggestion(const OmniboxSuggestion& other);
  ~OmniboxSuggestion();

  std::u16string contents;
  std::u16string description;
  AutocompleteMatch::ACMatchClassifications contents_classifications;
  AutocompleteMatch::ACMatchClassifications description_classifications;
  // This field is not a raw_ptr<> because of problems related to lambdas with
  // no return type, where the return value is raw_ptr<T>, but
  // variable/parameter receiving the lambda.
  RAW_PTR_EXCLUSION const gfx::VectorIcon* icon = nullptr;
  GURL destination;
  Autocompletion autocompletion;
};

// This struct contains the minimal set of information required to construct an
// AutocompleteInput on VR's behalf.
struct VR_BASE_EXPORT AutocompleteRequest {
  std::u16string text;
  size_t cursor_position = 0;
  bool prevent_inline_autocomplete = false;

  bool operator==(const AutocompleteRequest& other) const {
    return text == other.text && cursor_position == other.cursor_position &&
           prevent_inline_autocomplete == other.prevent_inline_autocomplete;
  }
  bool operator!=(const AutocompleteRequest& other) const {
    return !(*this == other);
  }
};

// This struct represents the current request to the AutocompleteController.
struct VR_BASE_EXPORT AutocompleteStatus {
  bool active = false;
  std::u16string input;

  bool operator==(const AutocompleteStatus& other) const {
    return active == other.active && input == other.input;
  }
  bool operator!=(const AutocompleteStatus& other) const {
    return !(*this == other);
  }
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_
