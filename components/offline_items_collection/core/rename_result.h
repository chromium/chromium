// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_RENAME_RESULT_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_RENAME_RESULT_H_

// The type of download rename dialog that should by shown by Android.
// A Java counterpart will be generated for this enum.
// Please treat this list as append only and keep it in sync with
// Android.Download.Rename.Result in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_items_collection
enum class RenameResult {
  SUCCESS = 0,                // Rename filename successfully
  FAILURE_NAME_CONFLICT = 1,  // Filename already exists
  FAILURE_NAME_TOO_LONG = 2,  // Illegal file name: too long
  FAILURE_NAME_INVALID = 3,   // Name invalid
  FAILURE_UNAVAILABLE = 4,    // Item unavailable
  FAILURE_UNKNOWN = 5,        // Unknown
  kMaxValue = FAILURE_UNKNOWN
};

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_RENAME_RESULT_H_
