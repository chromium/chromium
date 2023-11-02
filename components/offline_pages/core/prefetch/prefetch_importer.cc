// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_importer.h"

#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"

namespace offline_pages {

PrefetchImporter::PrefetchImporter(PrefetchDispatcher* dispatcher)
    : dispatcher_(dispatcher) {}

void PrefetchImporter::NotifyArchiveImported(int64_t offline_id, bool success) {
  dispatcher_->ArchiveImported(offline_id, success);
}

}  // namespace offline_pages