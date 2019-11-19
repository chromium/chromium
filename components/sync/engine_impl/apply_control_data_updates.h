// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_APPLY_CONTROL_DATA_UPDATES_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_APPLY_CONTROL_DATA_UPDATES_H_

// TODO(mamir): rename this file to apply_nigori_update.h
namespace syncer {

namespace syncable {
class Directory;
}

void ApplyNigoriUpdate(syncable::Directory* dir);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_APPLY_CONTROL_DATA_UPDATES_H_
