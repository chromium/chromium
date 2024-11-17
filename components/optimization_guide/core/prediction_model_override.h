// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_

#include <cstddef>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace optimization_guide {

// Return the separator used in the model override switch.
std::string ModelOverrideSeparator();

// Holds command-line overrides for prediction models.
class PredictionModelOverrides {
 public:
  struct Entry {
   public:
    using BuiltCallback =
        base::OnceCallback<void(std::unique_ptr<proto::PredictionModel>)>;
    Entry(proto::OptimizationTarget target,
          base::FilePath path,
          std::optional<proto::Any> metadata = std::nullopt);
    Entry(Entry&&);
    ~Entry();

    // Attempts to parse the Entry into a |proto::PredictionModel|.
    // Returns the result (or nullptr if there was an error) via `callback`.
    // In the event of an error, check LOG(ERROR).
    void BuildModel(const base::FilePath& base_model_dir,
                    BuiltCallback callback) const;

    proto::OptimizationTarget target() const { return target_; }
    base::FilePath path() const { return path_; }
    std::optional<proto::Any> metadata() const { return metadata_; }

   private:
    proto::OptimizationTarget target_;
    // An absolute path to the model file.
    base::FilePath path_;
    // The provided metadata for the model.
    std::optional<proto::Any> metadata_ = std::nullopt;
  };

  ~PredictionModelOverrides();
  PredictionModelOverrides(PredictionModelOverrides&&);

  // Get the override entry for the given optimization target. Returns nullptr
  // if there is no override for the given optimization target.
  const Entry* Get(proto::OptimizationTarget optimization_target) const;

  size_t size() const { return overrides_.size(); }

  static PredictionModelOverrides ParseFromCommandLine(
      base::CommandLine* command_line);

 private:
  explicit PredictionModelOverrides(std::vector<Entry> overrides);

  std::map<proto::OptimizationTarget, Entry> overrides_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_
