// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/test_prefetch_importer.h"

namespace offline_pages {

TestPrefetchImporter::TestPrefetchImporter() : PrefetchImporter(nullptr) {}

TestPrefetchImporter::~TestPrefetchImporter() = default;

void TestPrefetchImporter::ImportArchive(const PrefetchArchiveInfo& archive) {}

void TestPrefetchImporter::MarkImportCompleted(int64_t offline_id) {
  latest_completed_offline_id.push_back(offline_id);
}

std::set<int64_t> TestPrefetchImporter::GetOutstandingImports() const {
  return std::set<int64_t>();
}

}  // namespace offline_pages
