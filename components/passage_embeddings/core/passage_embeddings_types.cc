// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/core/passage_embeddings_types.h"

#include <algorithm>

#include "base/check.h"
#include "base/not_fatal_until.h"

namespace passage_embeddings {

Embedding::Embedding(std::vector<float> data) : data_(std::move(data)) {
  CHECK(!data_.empty(), base::NotFatalUntil::M152);
}

Embedding::~Embedding() = default;
Embedding::Embedding(const Embedding&) = default;
Embedding& Embedding::operator=(const Embedding&) = default;
Embedding::Embedding(Embedding&&) = default;
Embedding& Embedding::operator=(Embedding&&) = default;

float Embedding::Magnitude() const {
  float sum = 0.0f;
  for (float s : data_) {
    sum += s * s;
  }
  return std::sqrt(sum);
}

void Embedding::Normalize() {
  float magnitude = Magnitude();
  if (std::abs(magnitude - 1) > 0.0001f) {
    CHECK_GT(magnitude, std::numeric_limits<float>::epsilon());
    for (float& s : data_) {
      s /= magnitude;
    }
  }
}

float Embedding::ScoreWith(const Embedding& other_embedding) const {
  // This check is redundant since the database layers ensure embeddings
  // always have a fixed consistent size, but code can change with time,
  // and being sure directly before use may eventually catch a bug.
  DCHECK_EQ(data_.size(), other_embedding.data_.size());

  float embedding_score = 0.0f;
  for (size_t i = 0; i < data_.size(); i++) {
    embedding_score += data_[i] * other_embedding.data_[i];
  }
  return embedding_score;
}

}  // namespace passage_embeddings
