// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_
#define CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_

#include <string>

#include "base/strings/string16.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "url/gurl.h"

namespace vr {

struct VR_BASE_EXPORT Autocompletion {
  Autocompletion();
  Autocompletion(const base::string16& new_input,
                 const base::string16& new_suffix);

  bool operator==(const Autocompletion& other) const;
  bool operator!=(const Autocompletion& other) const {
    return !(*this == other);
  }

  // Input string that yielded the autocomplete text.
  base::string16 input;
  // The suffix to be appended to |input| to generate a complete match.
  base::string16 suffix;
};

struct VR_BASE_EXPORT OmniboxSuggestion {
  OmniboxSuggestion();

  OmniboxSuggestion(const base::string16& new_contents,
                    const base::string16& new_description,
                    const AutocompleteMatch::ACMatchClassifications&
                        new_contents_classifications,
                    const AutocompleteMatch::ACMatchClassifications&
                        new_description_classifications,
                    const gfx::VectorIcon* icon,
                    GURL new_destination,
                    const base::string16& new_input,
                    const base::string16& new_inline_autocompletion);
  OmniboxSuggestion(const OmniboxSuggestion& other);
  ~OmniboxSuggestion();

  base::string16 contents;
  base::string16 description;
  AutocompleteMatch::ACMatchClassifications contents_classifications;
  AutocompleteMatch::ACMatchClassifications description_classifications;
  const gfx::VectorIcon* icon = nullptr;
  GURL destination;
  Autocompletion autocompletion;
};

// This struct contains the minimal set of information required to construct an
// AutocompleteInput on VR's behalf.
struct VR_BASE_EXPORT AutocompleteRequest {
  base::string16 text;
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
  base::string16 input;

  bool operator==(const AutocompleteStatus& other) const {
    return active == other.active && input == other.input;
  }
  bool operator!=(const AutocompleteStatus& other) const {
    return !(*this == other);
  }
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_OMNIBOX_SUGGESTIONS_H_
