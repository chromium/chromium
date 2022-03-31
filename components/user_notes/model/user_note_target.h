// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_MODEL_USER_NOTE_TARGET_H_
#define COMPONENTS_USER_NOTES_MODEL_USER_NOTE_TARGET_H_

#include <string>

#include "url/gurl.h"

namespace user_notes {

// Model class for a note target.
class UserNoteTarget {
 public:
  enum TargetType { PAGE = 0, PAGE_TEXT };

  explicit UserNoteTarget(TargetType type,
                          const std::string& original_text,
                          GURL target_page,
                          const std::string& selector);
  ~UserNoteTarget();
  UserNoteTarget(const UserNoteTarget&) = delete;
  UserNoteTarget& operator=(const UserNoteTarget&) = delete;

  TargetType type() const { return type_; }
  const std::string& original_text() const { return original_text_; }
  const GURL& target_page() const { return target_page_; }
  const std::string& selector() const { return selector_; }

 private:
  // The type of target. Currently only page and page text is supported.
  TargetType type_;

  // The original text to which the note was attached. Useful if the page
  // changes. Empty for `TargetType::PAGE`.
  std::string original_text_;

  // The URL of the page the note is attached to.
  GURL target_page_;

  // The text fragment selector that identifies the `original_text_`.
  // Empty for `TargetType::PAGE`.
  std::string selector_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_MODEL_USER_NOTE_TARGET_H_
