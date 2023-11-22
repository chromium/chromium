// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/ime_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/ash/services/ime/constants.h"
#include "chromeos/ash/services/ime/decoder/decoder_engine.h"
#include "chromeos/ash/services/ime/decoder/system_engine.h"
#include "mojo/public/c/system/thunks.h"

namespace ash {
namespace ime {

namespace {

// Compose a relative FilePath beased on a C-string path.
base::FilePath RelativePathFromCStr(const char* path) {
  // Target path MUST be relative for security concerns.
  base::FilePath initial_path(path);
  base::FilePath relative_path(kInputMethodsDirName);
  return relative_path.Append(kLanguageDataDirName)
      .Append(initial_path.BaseName());
}

// Convert a final downloaded file path to a allowlisted path in string format.
std::string ResolveDownloadPath(const base::FilePath& file) {
  base::FilePath target(kUserInputMethodsDirPath);
  target = target.Append(kLanguageDataDirName).Append(file.BaseName());
  return target.MaybeAsASCII();
}

}  // namespace

std::string FieldTrialParamsRetrieverImpl::GetFieldTrialParamValueByFeature(
    const base::Feature& feature,
    const std::string& param_name) {
  return base::GetFieldTrialParamValueByFeature(feature, param_name);
}

ImeService::ImeService(
    mojo::PendingReceiver<mojom::ImeService> receiver,
    ImeSharedLibraryWrapper* ime_shared_library_wrapper,
    std::unique_ptr<FieldTrialParamsRetriever> field_trial_params_retriever)
    : receiver_(this, std::move(receiver)),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      ime_shared_library_(ime_shared_library_wrapper),
      field_trial_params_retriever_(std::move(field_trial_params_retriever)) {}

ImeService::~ImeService() = default;

void ImeService::SetPlatformAccessProvider(
    mojo::PendingRemote<mojom::PlatformAccessProvider> provider) {
  platform_access_.Bind(std::move(provider));
}

void ImeService::BindInputEngineManager(
    mojo::PendingReceiver<mojom::InputEngineManager> receiver) {
  manager_receivers_.Add(this, std::move(receiver));
}

void ImeService::ResetAllBackendConnections() {
  decoder_engine_.reset();
  system_engine_.reset();
}

void ImeService::ConnectToImeEngine(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> to_engine_request,
    mojo::PendingRemote<mojom::InputChannel> from_engine,
    const std::vector<uint8_t>& extra,
    ConnectToImeEngineCallback callback) {
  // There can only be one client using the decoder at any time. There are two
  // possible clients: NativeInputMethodEngine (for physical keyboard) and the
  // XKB extension (for virtual keyboard). The XKB extension may try to
  // connect the decoder even when it's not supposed to (due to race
  // conditions), so we must prevent the extension from taking over the
  // NativeInputMethodEngine connection.
  //
  // The extension will only use ConnectToImeEngine, and NativeInputMethodEngine
  // will only use ConnectToInputMethod.
  if (system_engine_ && system_engine_->IsConnected()) {
    std::move(callback).Run(/*bound=*/false);
    return;
  }

  ResetAllBackendConnections();

  decoder_engine_ = std::make_unique<DecoderEngine>(
      this, ime_shared_library_->MaybeLoadThenReturnEntryPoints());
  bool bound = decoder_engine_->BindRequest(
      ime_spec, std::move(to_engine_request), std::move(from_engine), extra);
  std::move(callback).Run(bound);
}

void ImeService::InitializeConnectionFactory(
    mojo::PendingReceiver<mojom::ConnectionFactory> connection_factory,
    InitializeConnectionFactoryCallback callback) {
  ResetAllBackendConnections();

  system_engine_ = std::make_unique<SystemEngine>(
      this, ime_shared_library_->MaybeLoadThenReturnEntryPoints());
  bool bound =
      system_engine_->BindConnectionFactory(std::move(connection_factory));
  std::move(callback).Run(bound);
}

const char* ImeService::GetImeBundleDir() {
  return kBundledInputMethodsDirPath;
}

void ImeService::Unused3() {
  NOTIMPLEMENTED();
}

const char* ImeService::GetImeUserHomeDir() {
  return kUserInputMethodsDirPath;
}

void ImeService::RunInMainSequence(ImeSequencedTask task, int task_id) {
  // This helps ensure that tasks run in the **main** SequencedTaskRunner.
  // E.g. the Mojo Remotes are bound on the main_task_runner_, so that any task
  // invoked Mojo call from other threads (sequences) should be posted to
  // main_task_runner_ by this function.
  main_task_runner_->PostTask(FROM_HERE, base::BindOnce(task, task_id));
}

bool ImeService::IsFeatureEnabled(const char* feature_name) {
  static const base::Feature* kConsideredFeatures[] = {
      &features::kAssistEmojiEnhanced,
      &features::kAssistMultiWord,
      &features::kAutocorrectParamsTuning,
      &features::kFirstPartyVietnameseInput,
      &ash::standalone_browser::features::kLacrosOnly,
      &features::kSystemJapanesePhysicalTyping,
      &features::kImeDownloaderUpdate,
      &features::kImeKoreanOnlyModeSwitchOnRightAlt,
      &features::kImeUsEnglishModelUpdate,
      &features::kImeFstDecoderParamsUpdate,
      &features::kAutocorrectByDefault,
      &features::kAutocorrectUseReplaceSurroundingText,
      &features::kInputMethodKoreanRightAltKeyDownFix,
      &features::kImeKoreanModeSwitchDebug,
  };

  // Use consistent feature flag names as in CrOS base::Feature::name and always
  // wire 1:1 to CrOS feature flags without extra logic.
  for (const base::Feature* feature : kConsideredFeatures) {
    if (strcmp(feature_name, feature->name) == 0) {
      return base::FeatureList::IsEnabled(*feature);
    }
  }

  // For backwards-compatibility, check for the "LacrosSupport" flag, which was
  // replaced by LacrosOnly.
  // TODO(b/290714161): Remove this once the shared library no longer uses
  // LacrosSupport.
  if (strcmp(feature_name, "LacrosSupport") == 0) {
    return base::FeatureList::IsEnabled(
        ash::standalone_browser::features::kLacrosOnly);
  }

  return false;
}

const char* ImeService::GetFieldTrialParamValueByFeature(
    const char* feature_name,
    const char* param_name) {
  char* c_string_value;

  if (strcmp(feature_name, features::kAutocorrectParamsTuning.name) == 0) {
    std::string string_value =
        field_trial_params_retriever_->GetFieldTrialParamValueByFeature(
            features::kAutocorrectParamsTuning, param_name);
    c_string_value =
        new char[string_value.length() + 1];  // extra slot for NULL '\0' char
    strcpy(c_string_value, string_value.c_str());
  } else {
    c_string_value = new char[1];
    c_string_value[0] = '\0';
  }

  return c_string_value;
}

void ImeService::Unused2() {
  NOTIMPLEMENTED();
}

int ImeService::SimpleDownloadToFileV2(const char* url,
                                       const char* file_path,
                                       SimpleDownloadCallbackV2 callback) {
  if (!platform_access_.is_bound()) {
    callback(SIMPLE_DOWNLOAD_STATUS_ABORTED, url, "");
    LOG(ERROR) << "Failed to download due to missing binding.";
  } else {
    platform_access_->DownloadImeFileTo(
        GURL(url), RelativePathFromCStr(file_path),
        base::BindOnce(&ImeService::SimpleDownloadFinishedV2,
                       base::Unretained(this), std::move(callback),
                       std::string(url)));
  }

  // For |SimpleDownloadToFileV2|, always returns 0.
  return 0;
}

void ImeService::SimpleDownloadFinishedV2(SimpleDownloadCallbackV2 callback,
                                          const std::string& url_str,
                                          const base::FilePath& file) {
  if (file.empty()) {
    callback(SIMPLE_DOWNLOAD_STATUS_INVALID_ARGUMENT, url_str.c_str(), "");
  } else {
    callback(SIMPLE_DOWNLOAD_STATUS_OK, url_str.c_str(),
             ResolveDownloadPath(file).c_str());
  }
}

const MojoSystemThunks* ImeService::GetMojoSystemThunks() {
  return MojoEmbedderGetSystemThunks32();
}

void ImeService::Unused1() {
  NOTIMPLEMENTED();
}

}  // namespace ime
}  // namespace ash
