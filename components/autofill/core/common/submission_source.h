// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_SUBMISSION_SOURCE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_SUBMISSION_SOURCE_H_

namespace autofill {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class SubmissionSource {
  NONE,                      // No submission signal was detected.
  SAME_DOCUMENT_NAVIGATION,  // The form was removed in same document
                             // navigation.
  XHR_SUCCEEDED,             // The form was removed whem XHR succeeded.
  FRAME_DETACHED,            // The subframe which has form was detached.
  DOM_MUTATION_AFTER_XHR,    // The form was removed after XHR.
  PROBABLY_FORM_SUBMITTED,   // The form was probably submitted since new page
                             // is loaded.
  FORM_SUBMISSION,           // Normal form submission.
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_SUBMISSION_SOURCE_H_
