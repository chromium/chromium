// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_STOP_METADATA_FATE_H_
#define COMPONENTS_SYNC_BASE_SYNC_STOP_METADATA_FATE_H_

namespace syncer {

// Passed as an argument when stopping sync to control whether models should
// clear its metadata (e.g. sync disabled vs browser shutdown).
enum SyncStopMetadataFate { KEEP_METADATA, CLEAR_METADATA };

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNC_STOP_METADATA_FATE_H_
