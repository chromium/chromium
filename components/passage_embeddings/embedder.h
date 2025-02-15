// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_EMBEDDER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_EMBEDDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace passage_embeddings {

class Embedding {
 public:
  explicit Embedding(std::vector<float> data);
  Embedding(std::vector<float> data, size_t passage_word_count);
  Embedding();
  ~Embedding();
  Embedding(const Embedding&);
  Embedding& operator=(const Embedding&);
  Embedding(Embedding&&);
  Embedding& operator=(Embedding&&);
  bool operator==(const Embedding&) const;

  // The number of elements in the data vector.
  size_t Dimensions() const;

  // The length of the vector.
  float Magnitude() const;

  // Scale the vector to unit length.
  void Normalize();

  // Compares one embedding with another and returns a similarity measure.
  float ScoreWith(const Embedding& other_embedding) const;

  // Const accessor used for storage.
  const std::vector<float>& GetData() const { return data_; }

  // Used for search filtering of passages with low word count.
  size_t GetPassageWordCount() const { return passage_word_count_; }
  void SetPassageWordCount(size_t passage_word_count) {
    passage_word_count_ = passage_word_count;
  }

 private:
  std::vector<float> data_;
  size_t passage_word_count_ = 0;
};

class EmbedderMetadataObserver : public base::CheckedObserver {
 public:
  // This is notified when model metadata is updated.
  virtual void EmbedderMetadataUpdated(EmbedderMetadata metadata) = 0;
};

// Base class that hides implementation details for how text is embedded.
class Embedder {
 public:
  using TaskId = uint64_t;
  static constexpr TaskId kInvalidTaskId = 0;

  virtual ~Embedder() = default;

  // Computes embeddings for each entry in `passages`. Will invoke callback on
  // done. If successful, it is guaranteed that the number of passages in
  // `passages` will match the number of entries in the embeddings vector and in
  // the same order. If unsuccessful, the callback will still return the
  // original passages but an empty embeddings vector.
  using ComputePassagesEmbeddingsCallback =
      base::OnceCallback<void(std::vector<std::string> passages,
                              std::vector<Embedding> embeddings,
                              TaskId task_id,
                              ComputeEmbeddingsStatus status)>;
  virtual TaskId ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) = 0;

  // Cancels computation of embeddings iff none of the passages given to
  // `ComputePassagesEmbeddings()` has been submitted for embedding yet.
  // If successful, the callback for the canceled task will be invoked with
  // `ComputeEmbeddingsStatus::kCanceled` status.
  virtual bool TryCancel(TaskId task_id) = 0;

 protected:
  Embedder() = default;
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_EMBEDDER_H_
