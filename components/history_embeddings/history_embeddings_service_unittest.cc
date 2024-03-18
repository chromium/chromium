// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

class HistoryEmbeddingsTest : public testing::Test {
 public:
  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());

    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
    CHECK(history_service_);
  }

  void TearDown() override {}

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
};

TEST_F(HistoryEmbeddingsTest, Constructs) {
  std::make_unique<HistoryEmbeddingsService>(history_dir_.GetPath(),
                                             history_service_.get());
}

}  // namespace history_embeddings
