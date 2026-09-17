// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_component_directory.h"

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"

namespace optimization_guide {

ManifestComponentDirectory::ManifestComponentDirectory(
    const proto::Manifest& manifest) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  Add(manifest);
}
ManifestComponentDirectory::~ManifestComponentDirectory() = default;

ManifestComponentDirectory& ManifestComponentDirectory::Add(
    const proto::Manifest& manifest) {
  CHECK(base::WriteFile(temp_dir_.GetPath().Append(kManifestFileName),
                        manifest.SerializeAsString()));
  return *this;
}

ManifestComponentDirectory& ManifestComponentDirectory::Add(
    const std::string& filename,
    const proto::SolutionConfig& config) {
  CHECK(base::WriteFile(temp_dir_.GetPath().AppendASCII(filename),
                        config.SerializeAsString()));
  return *this;
}

ManifestOverrideBuilder::ManifestOverrideBuilder() = default;
ManifestOverrideBuilder::~ManifestOverrideBuilder() = default;

ManifestOverrideBuilder& ManifestOverrideBuilder::SetManifestPath(
    const base::FilePath& path) {
  dict_.Set("manifest_path", path.AsUTF8Unsafe());
  return *this;
}

ManifestOverrideBuilder& ManifestOverrideBuilder::AddComponentOverride(
    const std::string& public_key_hex,
    const std::string& version,
    const base::FilePath& path) {
  dict_.EnsureDict("components")
      ->EnsureDict(public_key_hex)
      ->Set(version, path.AsUTF8Unsafe());
  return *this;
}

base::DictValue ManifestOverrideBuilder::Build() {
  return std::move(dict_);
}

ManifestOverrideFile::ManifestOverrideFile(base::DictValue dict) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  std::string json_content;
  CHECK(base::JSONWriter::Write(dict, &json_content));
  CHECK(base::WriteFile(path(), json_content));
}

ManifestOverrideFile::~ManifestOverrideFile() = default;

base::FilePath ManifestOverrideFile::path() const {
  return temp_dir_.GetPath().AppendASCII("override.json");
}

}  // namespace optimization_guide
