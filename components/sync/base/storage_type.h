// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_STORAGE_TYPE_H_
#define COMPONENTS_SYNC_BASE_STORAGE_TYPE_H_

namespace syncer {

// Represents whether the nature of a local database in terms of data ownership.
enum class StorageType {
  // The nature of the data in storage is not attributed explicitly to an
  // account. This is the default and the one used for sync-the-feature, as
  // there is no clear separation between local and remote storage.
  kUnspecified,
  // Account storage indicates all data can be attributed to a server-side
  // account, which also means the data will be removed from local storage when
  // the user signs out.
  kAccount,
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_STORAGE_TYPE_H_
