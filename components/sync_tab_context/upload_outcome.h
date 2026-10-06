// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_UPLOAD_OUTCOME_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_UPLOAD_OUTCOME_H_

namespace sync_tab_context {

// Outcome of an asynchronous page context upload operation.
enum class UploadOutcome {
  kSucceeded,
  kFailed,
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_UPLOAD_OUTCOME_H_
