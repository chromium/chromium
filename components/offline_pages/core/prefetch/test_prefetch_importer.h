// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_IMPORTER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_IMPORTER_H_

#include <vector>

#include "components/offline_pages/core/prefetch/prefetch_importer.h"

namespace offline_pages {

// Testing prefetch importer that does nothing.
class TestPrefetchImporter : public PrefetchImporter {
 public:
  TestPrefetchImporter();
  ~TestPrefetchImporter() override;

  void ImportArchive(const PrefetchArchiveInfo& archive) override;
  void MarkImportCompleted(int64_t offline_id) override;
  std::set<int64_t> GetOutstandingImports() const override;

  std::vector<int64_t> latest_completed_offline_id;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_IMPORTER_H_