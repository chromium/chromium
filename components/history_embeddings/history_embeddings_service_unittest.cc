// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

class HistoryEmbeddingsTest : public testing::Test {
 public:
  void SetUp() override { CHECK(history_dir_.CreateUniqueTempDir()); }

  void TearDown() override {}

 protected:
  base::ScopedTempDir history_dir_;
};

TEST_F(HistoryEmbeddingsTest, Constructs) {
  std::make_unique<HistoryEmbeddingsService>(history_dir_.GetPath());
}

}  // namespace history_embeddings
