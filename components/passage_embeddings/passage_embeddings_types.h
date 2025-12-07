// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TYPES_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TYPES_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"

namespace passage_embeddings {

inline constexpr char kModelInfoMetricName[] =
    "History.Embeddings.Embedder.ModelInfoStatus";

enum class EmbeddingsModelInfoStatus {
  kUnknown = 0,

  // Model info is valid.
  kValid = 1,

  // Model info is empty.
  kEmpty = 2,

  // Model info does not contain model metadata.
  kNoMetadata = 3,

  // Model info has invalid metadata.
  kInvalidMetadata = 4,

  // Model info has invalid additional files.
  kInvalidAdditionalFiles = 5,

  // This must be kept in sync with EmbeddingsModelInfoStatus in
  // history/enums.xml.
  kMaxValue = kInvalidAdditionalFiles,
};

// Classifies the priority of embeddings generation requests.
enum PassagePriority {
  // Executed as quickly as possible, runs faster and costs more resources.
  kUserInitiated = 0,

  // Executes quickly but possibly at lower cost than kUserInitiated.
  kUrgent = 1,

  // Execution is deprioritized and runs more slowly but more economically.
  kPassive = 2,

  // Execution may be delayed indefinitely and runs economically.
  kLatent = 3,
};

// The status of an embeddings generation attempt.
enum class ComputeEmbeddingsStatus {
  // Embeddings are generated successfully.
  kSuccess = 0,

  // The model files required for generation are not available.
  kModelUnavailable = 1,

  // Failure occurred during model execution.
  kExecutionFailure = 2,

  // The generation request was canceled, either explicitly or due to limits.
  kCanceled = 3,

  // This must be kept in sync with ComputeEmbeddingsStatus in
  // history/enums.xml.
  kMaxValue = kCanceled,
};

struct EmbedderMetadata {
  EmbedderMetadata(int64_t model_version,
                   size_t output_size,
                   std::optional<double> search_score_threshold = std::nullopt)
      : model_version(model_version),
        output_size(output_size),
        search_score_threshold(search_score_threshold) {}

  bool IsValid() { return model_version != 0 && output_size != 0; }

  int64_t model_version;
  size_t output_size;
  std::optional<double> search_score_threshold;
};

// Observer interface for getting notified when the embedder metadata is updated.
class EmbedderMetadataObserver : public base::CheckedObserver {
 public:
  // Called when the embedder metadata is updated.
  virtual void EmbedderMetadataUpdated(EmbedderMetadata metadata) = 0;
};

// Notifies observers when the embedder metadata is updated.
class EmbedderMetadataProvider {
 public:
  virtual ~EmbedderMetadataProvider() = default;

  // Subscribes `observer` for notifications when the embedder metadata is
  // updated. Will immediately notify if metadata is ready at the time of call.
  virtual void AddObserver(EmbedderMetadataObserver* observer) = 0;
  // Unsubscribes `observer` from notifications when the embedder metadata is
  // updated.
  virtual void RemoveObserver(EmbedderMetadataObserver* observer) = 0;

 protected:
  EmbedderMetadataProvider() = default;
};

// Encapsulate embeddings and related helpers.
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

// Computes embeddings for passages. Allows for cancellation of tasks.
class Embedder {
 public:
  using TaskId = uint64_t;

  virtual ~Embedder() = default;

  // Computes embeddings for each entry in `passages`. Will invoke `callback`
  // when done. If successful, it is guaranteed that the callback will return
  // the same number of passages and embeddings and in the same order as
  // `passages`. Otherwise the callback will return the original passages but
  // with an empty embeddings vector.
  using ComputePassagesEmbeddingsCallback =
      base::OnceCallback<void(std::vector<std::string> passages,
                              std::vector<Embedding> embeddings,
                              TaskId task_id,
                              ComputeEmbeddingsStatus status)>;
  virtual TaskId ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) = 0;

  // Updates all pending tasks to have the specified priority.
  virtual void ReprioritizeTasks(PassagePriority priority,
                                 const std::set<TaskId>& tasks) = 0;

  // Cancels computation of embeddings iff none of the passages given to
  // `ComputePassagesEmbeddings()` has been submitted for embedding yet.
  // If successful, the callback for the canceled task will be invoked with
  // `ComputeEmbeddingsStatus::kCanceled` status.
  virtual bool TryCancel(TaskId task_id) = 0;

 protected:
  Embedder() = default;
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TYPES_H_
