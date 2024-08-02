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
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_TYPES_H_
