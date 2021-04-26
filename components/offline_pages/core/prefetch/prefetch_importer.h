// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IMPORTER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IMPORTER_H_

#include <set>

#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

class PrefetchDispatcher;

// Interface to import the downloaded archive such that it can be rendered as
// offline page.
class PrefetchImporter {
 public:
  explicit PrefetchImporter(PrefetchDispatcher* dispatcher);
  virtual ~PrefetchImporter() = default;

  // Imports the downloaded archive by moving the file into archive directory
  // and creating an entry in the offline metadata database. When the import
  // is done, successfully or not, PrefetchDispatcher::ArchiveImported will be
  // called. MarkImportCompleted should then be called to remove the import from
  // the outstanding list once dispatch finishes processing it.
  virtual void ImportArchive(const PrefetchArchiveInfo& info) = 0;

  // Marks an import with |offline_id| as completed.
  // Note that this should be done inside a task, or reconciliation could be
  // affected by races.
  virtual void MarkImportCompleted(int64_t offline_id) = 0;

  // Returns a list of offline ids of those imports that are still not
  // completely done. An import will be removed from this list only after
  // the consumer reacts to the archive imported event and calls
  // MarkImportCompleted.
  virtual std::set<int64_t> GetOutstandingImports() const = 0;

 protected:
  void NotifyArchiveImported(int64_t offline_id, bool success);

 private:
  PrefetchDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchImporter);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IMPORTER_H_
