// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_HTML_BASED_USERNAME_DETECTOR_VOCABULARY_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_HTML_BASED_USERNAME_DETECTOR_VOCABULARY_H_

#include <stddef.h>

namespace autofill {

// Words that certainly point to a non-username field.
// If field values contain at least one negative word, then the field is
// excluded from the list of possible usernames.
extern const char* const kNegativeLatin[];
extern const size_t kNegativeLatinSize;
extern const char* const kNegativeNonLatin[];
extern const size_t kNegativeNonLatinSize;

// Translations of "username".
extern const char* const kUsernameLatin[];
extern const size_t kUsernameLatinSize;
extern const char* const kUsernameNonLatin[];
extern const size_t kUsernameNonLatinSize;

// Translations of "user".
extern const char* const kUserLatin[];
extern const size_t kUserLatinSize;
extern const char* const kUserNonLatin[];
extern const size_t kUserNonLatinSize;

// Words that certainly point to a username field, if they appear in developer
// value. They are technical words, because they can only be used as variable
// names, and not as stand-alone words.
extern const char* const kTechnicalWords[];
extern const size_t kTechnicalWordsSize;

// Words that might point to a username field.They have the smallest priority
// in the heuristic, because there are also field attribute values that
// contain them, but are not username fields.
extern const char* const kWeakWords[];
extern const size_t kWeakWordsSize;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_HTML_BASED_USERNAME_DETECTOR_VOCABULARY_H_
