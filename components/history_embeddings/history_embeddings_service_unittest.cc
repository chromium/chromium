// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

TEST(HistoryEmbeddingsTest, Constructs) {
  std::make_unique<HistoryEmbeddingsService>();
}

}  // namespace history_embeddings
