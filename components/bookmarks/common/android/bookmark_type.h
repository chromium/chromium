// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_TYPE_H_
#define COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_TYPE_H_

namespace bookmarks {

// A Java counterpart will be generated for this enum.
// New enum values should only be added to the end of the enum and no values
// should be modified or reused, as this is reported via UMA.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.bookmarks
enum BookmarkType {
  BOOKMARK_TYPE_NORMAL,
  BOOKMARK_TYPE_PARTNER,
  BOOKMARK_TYPE_READING_LIST,
  // BOOKMARK_TYPE_LAST must be the last element.
  BOOKMARK_TYPE_LAST = BOOKMARK_TYPE_READING_LIST,
};
}

#endif  // COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_TYPE_H_
