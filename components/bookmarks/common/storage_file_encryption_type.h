// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_STORAGE_FILE_ENCRYPTION_TYPE_H_
#define COMPONENTS_BOOKMARKS_COMMON_STORAGE_FILE_ENCRYPTION_TYPE_H_

// An enum class representing the type of encryption applied to the bookmarks
// data file.
enum class StorageFileEncryptionType {
  // Clear text, no encryption.
  kClearText,
  // Encryption is used.
  kEncrypted,
};

#endif  // COMPONENTS_BOOKMARKS_COMMON_STORAGE_FILE_ENCRYPTION_TYPE_H_
