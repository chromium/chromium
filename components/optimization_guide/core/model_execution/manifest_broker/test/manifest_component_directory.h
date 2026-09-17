// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_MANIFEST_COMPONENT_DIRECTORY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_MANIFEST_COMPONENT_DIRECTORY_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "components/optimization_guide/core/model_execution/configs/manifest_builder.h"
#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

// Constructs a Manifest component directory.
class ManifestComponentDirectory {
 public:
  explicit ManifestComponentDirectory(const proto::Manifest& manifest);
  ~ManifestComponentDirectory();

  // Replaces the manifest in the directory.
  ManifestComponentDirectory& Add(const proto::Manifest& manifest);
  // Adds a new solution config to the directory, overwriting existing ones.
  ManifestComponentDirectory& Add(const std::string& filename,
                                  const proto::SolutionConfig& config);

  base::FilePath path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Builder for the override JSON dict.
class ManifestOverrideBuilder {
 public:
  ManifestOverrideBuilder();
  ~ManifestOverrideBuilder();

  ManifestOverrideBuilder& SetManifestPath(const base::FilePath& path);
  ManifestOverrideBuilder& AddComponentOverride(
      const std::string& public_key_hex,
      const std::string& version,
      const base::FilePath& path);

  base::DictValue Build();

 private:
  base::DictValue dict_;
};

// Constructs an override JSON file from a Dict.
class ManifestOverrideFile {
 public:
  explicit ManifestOverrideFile(base::DictValue dict);
  ~ManifestOverrideFile();

  base::FilePath path() const;

 private:
  base::ScopedTempDir temp_dir_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_MANIFEST_COMPONENT_DIRECTORY_H_
