// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/sequenced_task_runner.h"
#include "build/buildflag.h"
#include "chromeos/services/ime/constants.h"
#include "chromeos/services/ime/decoder/decoder_engine.h"
#include "chromeos/services/ime/decoder/system_engine.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"
#include "chromeos/services/ime/rule_based_engine.h"

namespace chromeos {
namespace ime {

namespace {

enum SimpleDownloadError {
  SIMPLE_DOWNLOAD_ERROR_OK = 0,
  SIMPLE_DOWNLOAD_ERROR_FAILED = -1,
  SIMPLE_DOWNLOAD_ERROR_ABORTED = -2,
};

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

ImeService::ImeService(mojo::PendingReceiver<mojom::ImeService> receiver)
    : receiver_(this, std::move(receiver)),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
}

ImeService::~ImeService() = default;

void ImeService::SetPlatformAccessProvider(
    mojo::PendingRemote<mojom::PlatformAccessProvider> provider) {
  platform_access_.Bind(std::move(provider));
}

void ImeService::BindInputEngineManager(
    mojo::PendingReceiver<mojom::InputEngineManager> receiver) {
  manager_receivers_.Add(this, std::move(receiver));
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
  // This is a hack to to determine whether a connection came from
  // NativeInputMethodEngine or the extension. NativeInputMethodEngine will
  // send some extra bytes, whereas the extension doesn't. Thus, we can
  // prevent the extension from taking over the NativeInputMethodEngine's
  // connection to the decoder. NativeInputMethodEngine will voluntarily give
  // up its connection when tswitching to tablet mode, allowing the extension
  // to connect again.
  // TODO(b/184115850): Create a separate Mojo API for NativeInputMethodEngine
  // so that we don't need to inspect `extra` to distinguish the client.
  if (is_privileged_connection_ && extra.size() == 0) {
    std::move(callback).Run(/*bound=*/false);
    return;
  }

  if (base::FeatureList::IsEnabled(
          chromeos::features::kSystemLatinPhysicalTyping)) {
    auto system_engine = std::make_unique<SystemEngine>(this);
    bool bound = system_engine->BindRequest(
        ime_spec, std::move(to_engine_request), std::move(from_engine),
        base::BindOnce(
            [](bool& is_decoder_receiver_connected) {
              is_decoder_receiver_connected = false;
            },
            std::ref(is_privileged_connection_)));
    if (bound) {
      is_privileged_connection_ = extra.size() > 0;
    }
    input_engine_ = std::move(system_engine);
    std::move(callback).Run(bound);
  } else {
    auto decoder_engine = std::make_unique<DecoderEngine>(this);
    bool bound = decoder_engine->BindRequest(
        ime_spec, std::move(to_engine_request), std::move(from_engine), extra);
    input_engine_ = std::move(decoder_engine);
    std::move(callback).Run(bound);
  }
}

void ImeService::ConnectToInputMethod(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> to_engine,
    ConnectToInputMethodCallback callback) {
  input_engine_ = RuleBasedEngine::Create(ime_spec, std::move(to_engine));
  std::move(callback).Run(/*bound=*/input_engine_ != nullptr);
}

const char* ImeService::GetImeBundleDir() {
  return kBundledInputMethodsDirPath;
}

const char* ImeService::GetImeGlobalDir() {
  // Global IME data is supported yet.
  NOTIMPLEMENTED();
  return "";
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
  if (strcmp(feature_name, "AssistiveMultiWord") == 0) {
    return base::FeatureList::IsEnabled(chromeos::features::kAssistMultiWord);
  }
  if (strcmp(feature_name, "SystemLatinPhysicalTyping") == 0) {
    return base::FeatureList::IsEnabled(
        chromeos::features::kSystemLatinPhysicalTyping);
  }
  return false;
}

int ImeService::SimpleDownloadToFile(const char* url,
                                     const char* file_path,
                                     SimpleDownloadCallback callback) {
  if (!platform_access_.is_bound()) {
    callback(SIMPLE_DOWNLOAD_ERROR_ABORTED, "");
    LOG(ERROR) << "Failed to download due to missing binding.";
  } else {
    platform_access_->DownloadImeFileTo(
        GURL(url), RelativePathFromCStr(file_path),
        base::BindOnce(&ImeService::SimpleDownloadFinished,
                       base::Unretained(this), std::move(callback)));
  }

  // For |SimpleDownloadToFile|, always returns 0.
  return 0;
}

void ImeService::SimpleDownloadFinished(SimpleDownloadCallback callback,
                                        const base::FilePath& file) {
  if (file.empty()) {
    callback(SIMPLE_DOWNLOAD_ERROR_FAILED, "");
  } else {
    callback(SIMPLE_DOWNLOAD_ERROR_OK, ResolveDownloadPath(file).c_str());
  }
}

int ImeService::SimpleDownloadToFileV2(const char* url,
                                       const char* file_path,
                                       SimpleDownloadCallbackV2 callback) {
  if (!platform_access_.is_bound()) {
    callback(SIMPLE_DOWNLOAD_ERROR_ABORTED, url, "");
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
    callback(SIMPLE_DOWNLOAD_ERROR_FAILED, url_str.c_str(), "");
  } else {
    callback(SIMPLE_DOWNLOAD_ERROR_OK, url_str.c_str(),
             ResolveDownloadPath(file).c_str());
  }
}

ImeCrosDownloader* ImeService::GetDownloader() {
  // TODO(https://crbug.com/837156): Create an ImeCrosDownloader based on its
  // specification defined in interfaces. The caller should free it after use.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ime
}  // namespace chromeos
