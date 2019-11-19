// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_USER_SHARE_H_
#define COMPONENTS_SYNC_SYNCABLE_USER_SHARE_H_

#include <memory>

namespace syncer {

namespace syncable {
class Directory;
}

// A UserShare encapsulates the syncable pieces that represent an authenticated
// user and their data (share).
// This encompasses all pieces required to build transaction objects on the
// syncable share.
struct UserShare {
  UserShare();
  ~UserShare();

  // The Directory itself, which is the parent of Transactions.
  std::unique_ptr<syncable::Directory> directory;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_USER_SHARE_H_
