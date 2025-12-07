// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_COMMON_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_IOS_COMMON_CONSTANTS_H_

// Represent the type of counted form submission that was reported by the
// renderer.
enum class CountedSubmissionType {
  // Form submission detected via a form "submit" event.
  kHtmlEvent,
  // Form submission detected via the `form.submit()` call wrapper.
  kProgrammatic,
  // Can't parse the form submission count report received from the renderer.
  // This can happen if one of the keys in the message body was corrupted.
  kCantParse,
  kMaxValue = kCantParse,
};

#endif  // COMPONENTS_AUTOFILL_IOS_COMMON_CONSTANTS_H_
