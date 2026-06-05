// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_TYPES_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_TYPES_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
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

// Observer interface for getting notified when the embedder metadata is
// updated.
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
//
// Invariants for Embeddings produced by passage_embeddings::Embedder:
// * Embeddings always have non-zero lengths.
// * Embeddings are always normalized to unit length (magnitude 1.0).
// * Embeddings produced in the same run of Chrome will have consistent lengths.
// * Embeddings produced in different runs of Chrome can have different lengths
//   only if the embeddings model version was changed.
class Embedding {
 public:
  explicit Embedding(std::vector<float> data);
  ~Embedding();
  Embedding(const Embedding&);
  Embedding& operator=(const Embedding&);
  Embedding(Embedding&&);
  Embedding& operator=(Embedding&&);

  // Compares one embedding with another and returns a similarity measure.
  //
  // Note: Even if embeddings correspond to semantically unrelated texts the
  // similarity could be substantially high (and this is not a bug). This
  // phenomenon is known as "embedding anisotropy": embedding vectors might
  // belong to a narrow cone (instead of spreading across the entire space),
  // causing unrelated words or texts to have high similarity.
  //
  // In practice this means:
  // - You should calibrate the usage of embeddings similarity score according
  //   to your use case (e.g., "Is 0.5 a good threshold for your use case?").
  //   Also, consider using a downstream model which would use embeddings as its
  //   inputs.
  // - Alternatively, whenever possible, instead of relying on the absolute
  //   value of similarity score - consider using it for sorting (ranking).
  float ScoreWith(const Embedding& other_embedding) const;

  // Scale the vector to unit length. Returns nullopt if the vector has
  // near-zero magnitude and cannot be normalized.
  static std::optional<std::vector<float>> Normalize(std::vector<float> data);

  // Const accessor used for storage.
  const std::vector<float>& GetData() const { return data_; }

 private:
  std::vector<float> data_;
};

// Computes embeddings for passages. Allows for cancellation of jobs.
class Embedder {
 public:
  // Move-only RAII handle for an embeddings generation job. Cancellation is
  // triggered on destruction if the job has not already completed.
  class Job {
   public:
    Job(base::WeakPtr<Embedder> embedder, uint64_t job_id);
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    Job(Job&&);
    Job& operator=(Job&&);
    ~Job();

    // Updates the priority of this job.
    void Reprioritize(PassagePriority priority);

    uint64_t id() const { return id_; }

   private:
    base::WeakPtr<Embedder> embedder_;
    uint64_t id_ = 0;
  };

  virtual ~Embedder() = default;

  // Computes embeddings for each entry in `passages`. Will invoke `callback`
  // when done. If successful, it is guaranteed that the callback will return
  // the same number of passages and embeddings and in the same order as
  // `passages`. Otherwise the callback will return the original passages but
  // with an empty embeddings vector.
  //
  // Requirements on the implementation of this interface:
  // * Embeddings must always have non-zero lengths.
  // * Embeddings must be normalized to unit length (magnitude 1.0).
  // * Embeddings produced in the same run of Chrome must have consistent
  //   lengths.
  // * Embeddings produced in different runs of Chrome can have different
  //   lengths only if the embeddings model version was changed.
  using ComputePassagesEmbeddingsCallback =
      base::OnceCallback<void(std::vector<std::string> passages,
                              std::vector<Embedding> embeddings,
                              uint64_t job_id,
                              ComputeEmbeddingsStatus status)>;
  [[nodiscard]] virtual Job ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) = 0;

  virtual base::WeakPtr<Embedder> GetWeakPtr() = 0;

  // Updates all pending jobs to have the specified priority.
  virtual void ReprioritizeJobs(PassagePriority priority,
                                const std::set<uint64_t>& job_ids) = 0;

  // Comparator for Embedder::Job by id, supporting heterogeneous lookup.
  struct JobIdComparator {
    using is_transparent = void;
    bool operator()(const Job& a, const Job& b) const;
    bool operator()(const Job& a, uint64_t b) const;
    bool operator()(uint64_t a, const Job& b) const;
  };

 protected:
  Embedder();

  // Cancels computation of embeddings iff none of the passages given to
  // `ComputePassagesEmbeddings()` has been submitted for embedding yet.
  // If successful, the callback for the canceled job will be invoked with
  // `ComputeEmbeddingsStatus::kCanceled` status.
  virtual bool TryCancel(uint64_t job_id) = 0;
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_TYPES_H_
