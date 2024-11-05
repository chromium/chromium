// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_override.h"

#include <initializer_list>
#include <sstream>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/prediction_model_download_manager.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/services/unzip/public/cpp/unzip.h"

#if BUILDFLAG(IS_IOS)
#include "components/services/unzip/in_process_unzipper.h"  // nogncheck
#else
#include "components/services/unzip/content/unzip_service.h"  // nogncheck
#endif

namespace optimization_guide {

namespace {

using BuiltCallback = PredictionModelOverrides::Entry::BuiltCallback;

// The ":" character is reserved in Windows as part of an absolute file path,
// e.g.: C:\model.tflite, so we use a different separator.
#if BUILDFLAG(IS_WIN)
const char kModelOverrideSeparator[] = "|";
#else
const char kModelOverrideSeparator[] = ":";
#endif

std::unique_ptr<proto::PredictionModel> ProcessModelOverrideOnBGThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& unzipped_dir_path) {
  // Unpack and verify model info file.
  base::FilePath model_info_path =
      unzipped_dir_path.Append(GetBaseFileNameForModelInfo());
  std::string binary_model_info_pb;
  if (!base::ReadFileToString(model_info_path, &binary_model_info_pb)) {
    LOG(ERROR) << "Failed to read " << FilePathToString(model_info_path);
    return nullptr;
  }
  proto::ModelInfo model_info;
  if (!model_info.ParseFromString(binary_model_info_pb)) {
    LOG(ERROR) << "Failed to parse " << FilePathToString(model_info_path);
    return nullptr;
  }

  if (!model_info.has_version() || !model_info.has_optimization_target()) {
    LOG(ERROR) << FilePathToString(model_info_path)
               << "is invalid because it does not contain a version and/or "
                  "optimization target";
    return nullptr;
  }

  if (model_info.optimization_target() != optimization_target) {
    LOG(ERROR) << FilePathToString(model_info_path)
               << "is invalid because it does not contain the correct "
                  "optimization target";
    return nullptr;
  }

  for (int i = 0; i < model_info.additional_files_size(); i++) {
    proto::AdditionalModelFile* additional_file =
        model_info.mutable_additional_files(i);

    base::FilePath additional_file_basename =
        *StringToFilePath(additional_file->file_path());
    base::FilePath additional_file_absolute =
        unzipped_dir_path.Append(additional_file_basename);
    additional_file->set_file_path(FilePathToString(additional_file_absolute));
  }

  std::unique_ptr<proto::PredictionModel> model =
      std::make_unique<proto::PredictionModel>();
  *model->mutable_model_info() = model_info;
  model->mutable_model()->set_download_url(
      FilePathToString(unzipped_dir_path.Append(GetBaseFileNameForModels())));

  return model;
}

void OnModelOverrideUnzipped(proto::OptimizationTarget optimization_target,
                             const base::FilePath& base_model_dir,
                             BuiltCallback callback,
                             bool success) {
  if (!success) {
    LOG(ERROR) << FilePathToString(base_model_dir) << " failed to unzip";
    std::move(callback).Run(nullptr);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ProcessModelOverrideOnBGThread, optimization_target,
                     base_model_dir),
      std::move(callback));
}

void OnModelOverrideVerified(proto::OptimizationTarget optimization_target,
                             const base::FilePath& passed_crx_file_path,
                             const base::FilePath& base_model_dir,
                             BuiltCallback callback,
                             bool is_verify_success) {
  if (!is_verify_success) {
    LOG(ERROR) << passed_crx_file_path << " failed verification";
    std::move(callback).Run(nullptr);
    return;
  }

#if BUILDFLAG(IS_IOS)
  auto unzipper = unzip::LaunchInProcessUnzipper();
#else
  auto unzipper = unzip::LaunchUnzipper();
#endif
  unzip::Unzip(std::move(unzipper), passed_crx_file_path, base_model_dir,
               unzip::mojom::UnzipOptions::New(), unzip::AllContents(),
               base::DoNothing(),
               base::BindOnce(&OnModelOverrideUnzipped, optimization_target,
                              base_model_dir, std::move(callback)));
}

std::optional<PredictionModelOverrides::Entry> ParseEntry(
    const std::string& model_override) {
  std::vector<std::string> override_parts =
      base::SplitString(model_override, kModelOverrideSeparator,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (override_parts.size() != 2 && override_parts.size() != 3) {
    // Input is malformed. Should be either
    // <target>:<path>:<base64_metadata> or <target>:<path>
    DLOG(ERROR) << "Invalid string format provided to the Model Override";
    return std::nullopt;
  }

  proto::OptimizationTarget target;
  if (!proto::OptimizationTarget_Parse(override_parts[0], &target)) {
    // Optimization target is invalid.
    DLOG(ERROR) << "Invalid optimization target provided to the Model Override";
    return std::nullopt;
  }

  auto path = StringToFilePath(override_parts[1]);
  if (!path || !path->IsAbsolute()) {
    DLOG(ERROR) << "Provided model file path must be absolute "
                << path.value_or(base::FilePath()).value();
    return std::nullopt;
  }

  if (override_parts.size() == 2) {
    return PredictionModelOverrides::Entry(target, *path, std::nullopt);
  }

  std::string binary_pb;
  if (!base::Base64Decode(override_parts[2], &binary_pb)) {
    DLOG(ERROR) << "Invalid base64 encoding of the Model Override";
    return std::nullopt;
  }
  proto::Any metadata;
  if (!metadata.ParseFromString(binary_pb)) {
    DLOG(ERROR) << "Invalid model metadata provided to the Model Override";
    return std::nullopt;
  }
  return PredictionModelOverrides::Entry(target, *path, std::move(metadata));
}

}  // namespace

std::string ModelOverrideSeparator() {
  return kModelOverrideSeparator;
}

PredictionModelOverrides::Entry::Entry(proto::OptimizationTarget target,
                                       base::FilePath path,
                                       std::optional<proto::Any> metadata)
    : target_(target), path_(std::move(path)), metadata_(std::move(metadata)) {}

PredictionModelOverrides::Entry::~Entry() = default;
PredictionModelOverrides::Entry::Entry(Entry&&) = default;

void PredictionModelOverrides::Entry::BuildModel(
    const base::FilePath& base_model_dir,
    PredictionModelOverrides::Entry::BuiltCallback callback) const {
  if (path_.MatchesFinalExtension(FILE_PATH_LITERAL(".crx3"))) {
    DVLOG(0) << "Attempting to parse the model override at " << path_.value()
             << " as a crx model package for "
             << GetStringNameForOptimizationTarget(target_);
    if (metadata_) {
      LOG(ERROR) << "Ignoring the metadata that was passed since a crx package "
                    "was given";
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(PredictionModelDownloadManager::VerifyDownload, path_,
                       base_model_dir,
                       /*delete_file_on_error=*/false),
        base::BindOnce(&OnModelOverrideVerified, target_, path_, base_model_dir,
                       std::move(callback)));
    return;
  }

  auto prediction_model = std::make_unique<proto::PredictionModel>();
  prediction_model->mutable_model_info()->set_optimization_target(target_);
  prediction_model->mutable_model_info()->set_version(123);
  if (metadata_) {
    *prediction_model->mutable_model_info()->mutable_model_metadata() =
        metadata_.value();
  }
  prediction_model->mutable_model()->set_download_url(path_.MaybeAsASCII());
  std::move(callback).Run(std::move(prediction_model));
}

PredictionModelOverrides::PredictionModelOverrides(
    std::vector<Entry> overrides) {
  for (auto& entry : overrides) {
    overrides_.emplace(entry.target(), std::move(entry));
  }
}
PredictionModelOverrides::~PredictionModelOverrides() = default;
PredictionModelOverrides::PredictionModelOverrides(PredictionModelOverrides&&) =
    default;

const PredictionModelOverrides::Entry* PredictionModelOverrides::Get(
    proto::OptimizationTarget target) const {
  auto it = overrides_.find(target);
  if (it == overrides_.end()) {
    return nullptr;
  }
  return &it->second;
}

// static
PredictionModelOverrides PredictionModelOverrides::ParseFromCommandLine(
    base::CommandLine* command_line) {
  std::string switch_value =
      command_line->GetSwitchValueASCII(switches::kModelOverride);
  if (switch_value.empty()) {
    return PredictionModelOverrides({});
  }
  std::vector<Entry> parsed;
  std::vector<std::string> model_overrides = base::SplitString(
      switch_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& model_override : model_overrides) {
    if (auto entry = ParseEntry(model_override); entry) {
      parsed.push_back(std::move(*entry));
    }
  }
  return PredictionModelOverrides(std::move(parsed));
}

}  // namespace optimization_guide
