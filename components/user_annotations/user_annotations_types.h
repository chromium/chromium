// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_

namespace user_annotations {

typedef int64_t EntryID;

struct Entry {
  // The row ID of this entry from the user annotations database. This is
  // immutable except when retrieving the row from the database.
  EntryID entry_id;

  // The key for this entry. Not necessarily unique.
  std::u16string key;

  // The value for this entry.
  std::u16string value;
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_
