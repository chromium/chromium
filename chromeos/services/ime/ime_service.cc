// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "build/buildflag.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/ime/constants.h"
#include "chromeos/services/ime/decoder/decoder_engine.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"

namespace chromeos {
namespace ime {

namespace {

enum SimpleDownloadError {
  SIMPLE_DOWNLOAD_ERROR_OK = 0,
  SIMPLE_DOWNLOAD_ERROR_FAILED = -1,
  SIMPLE_DOWNLOAD_ERROR_ABORTED = -2,
};

}  // namespace

ImeService::ImeService(mojo::PendingReceiver<mojom::ImeService> receiver)
    : receiver_(this, std::move(receiver)),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  input_engine_ = chromeos::features::IsImeDecoderWithSandboxEnabled()
                      ? std::make_unique<DecoderEngine>(this)
                      : std::make_unique<InputEngine>();
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
  DCHECK(input_engine_);
  bool bound = input_engine_->BindRequest(
      ime_spec, std::move(to_engine_request), std::move(from_engine), extra);
  std::move(callback).Run(bound);
}

void ImeService::SimpleDownloadFinished(SimpleDownloadCallback callback,
                                        const base::FilePath& file) {
  if (file.empty()) {
    callback(SIMPLE_DOWNLOAD_ERROR_FAILED, "");
  } else {
    // Convert downloaded file path to an whitelisted path.
    base::FilePath target(kUserInputMethodsDirPath);
    target = target.Append(kLanguageDataDirName).Append(file.BaseName());
    callback(SIMPLE_DOWNLOAD_ERROR_OK, target.MaybeAsASCII().c_str());
  }
}

const char* ImeService::GetImeBundleDir() {
  return kBundledInputMethodsDirPath;
}

const char* ImeService::GetImeGlobalDir() {
  // Global IME data is supported yet.
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

int ImeService::SimpleDownloadToFile(const char* url,
                                     const char* file_path,
                                     SimpleDownloadCallback callback) {
  if (!platform_access_.is_bound()) {
    callback(SIMPLE_DOWNLOAD_ERROR_ABORTED, "");
    LOG(ERROR) << "Failed to download due to missing binding.";
  } else {
    // Target path MUST be relative for security concerns.
    // Compose a relative download path beased on the request.
    base::FilePath initial_path(file_path);
    base::FilePath relative_path(kInputMethodsDirName);
    relative_path = relative_path.Append(kLanguageDataDirName)
                        .Append(initial_path.BaseName());

    GURL download_url(url);
    platform_access_->DownloadImeFileTo(
        download_url, relative_path,
        base::BindOnce(&ImeService::SimpleDownloadFinished,
                       base::Unretained(this), std::move(callback)));
  }

  // For |SimpleDownloadToFile|, always returns 0.
  return 0;
}

ImeCrosDownloader* ImeService::GetDownloader() {
  // TODO(https://crbug.com/837156): Create an ImeCrosDownloader based on its
  // specification defined in interfaces. The caller should free it after use.
  return nullptr;
}

}  // namespace ime
}  // namespace chromeos
