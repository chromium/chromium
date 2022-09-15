// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/libassistant_loader_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace chromeos::libassistant {

namespace {

using InstallResult = chromeos::assistant::LibassistantDlcInstallResult;
using LoadStatus = chromeos::assistant::LibassistantDlcLoadStatus;

base::TaskTraits GetTaskTraits() {
  return {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
}

inline constexpr char kDlcInstallResultHistogram[] =
    "Assistant.Libassistant.DlcInstallResult";

inline constexpr char kDlcLoadStatusHistogram[] =
    "Assistant.Libassistant.DlcLoadStatus";

// The DLC ID of Libassistant.so, used to download and mount the library.
constexpr char kLibassistantDlcId[] = "assistant-dlc";

// On linux-chromeos, will load from `root_out_dir`.
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
inline constexpr char kLibassistantPath[] = "opt/google/chrome/libassistant.so";
#else
inline constexpr char kLibassistantPath[] = "libassistant.so";
#endif

base::FilePath GetLibassisantPath(const std::string& dlc_path) {
  return base::FilePath(dlc_path).Append(kLibassistantPath);
}

void RecordLibassistantDlcInstallResult(
    const DlcserviceClient::InstallResult& result) {
  InstallResult install_result = InstallResult::kErrorInternal;
  if (result.error == dlcservice::kErrorNone) {
    install_result = InstallResult::kSuccess;
  }
  if (result.error == dlcservice::kErrorInternal) {
    install_result = InstallResult::kErrorInternal;
  }
  if (result.error == dlcservice::kErrorBusy) {
    install_result = InstallResult::kErrorBusy;
  }
  if (result.error == dlcservice::kErrorNeedReboot) {
    install_result = InstallResult::kErrorNeedReboot;
  }
  if (result.error == dlcservice::kErrorInvalidDlc) {
    install_result = InstallResult::kErrorInvalidDlc;
  }
  if (result.error == dlcservice::kErrorAllocation) {
    install_result = InstallResult::kErrorAllocation;
  }
  if (result.error == dlcservice::kErrorNoImageFound) {
    install_result = InstallResult::kErrorNoImageFound;
  }
  base::UmaHistogramEnumeration(kDlcInstallResultHistogram, install_result);
}

void RecordLibassistantDlcLoadStatus(const LoadStatus& status) {
  base::UmaHistogramEnumeration(kDlcLoadStatusHistogram, status);
}

}  // namespace

void LibassistantLoaderImpl::Load(LoadCallback callback) {
  // If V2 flag is enabled or libassistant DLC flag is not enabled, will
  // fallback to load libassistant.so from rootfs.
  if (chromeos::assistant::features::IsLibAssistantV2Enabled() ||
      !chromeos::assistant::features::IsLibAssistantDlcEnabled()) {
    std::move(callback).Run(/*success=*/true);
    return;
  }

  if (entry_point_) {
    std::move(callback).Run(/*success=*/true);
    return;
  }

  InstallDlc(std::move(callback));
}

EntryPoint* LibassistantLoaderImpl::GetEntryPoint() {
  return entry_point_.get();
}

LibassistantLoaderImpl::LibassistantLoaderImpl()
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits())) {}

LibassistantLoaderImpl::~LibassistantLoaderImpl() = default;

void LibassistantLoaderImpl::InstallDlc(LoadCallback callback) {
  callback_ = std::move(callback);

  // Install libassistant.so from DLC.
  auto* client = DlcserviceClient::Get();
  if (!client) {
    DVLOG(1) << "DlcService client is not available";
    RunCallback(/*success=*/false);
    return;
  }

  DVLOG(1) << "Installing libassistant.so from DLC";
  dlcservice::InstallRequest install_request;
  install_request.set_id(kLibassistantDlcId);
  client->Install(install_request,
                  base::BindOnce(&LibassistantLoaderImpl::OnInstallDlcComplete,
                                 weak_factory_.GetWeakPtr()),
                  /*ProgressCallback=*/base::DoNothing());
}

void LibassistantLoaderImpl::OnInstallDlcComplete(
    const DlcserviceClient::InstallResult& result) {
  RecordLibassistantDlcInstallResult(result);

  if (result.error != dlcservice::kErrorNone) {
    DVLOG(1) << "Failed to install libassistant.so from DLC: " << result.error;
    RunCallback(/*success=*/false);
    return;
  }

  // `ScopedNativeLibrary` will call `LoadNativeLibraryWithOptions()`, which is
  // a blocking call. We need to send to a background thread to load it.
  base::FilePath path = GetLibassisantPath(result.root_path);
  DVLOG(3) << "Loading libassistant.so DLC from: " << path;
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) {
            return base::ScopedNativeLibrary(path);
          },
          path),
      base::BindOnce(&LibassistantLoaderImpl::OnLibraryLoaded,
                     weak_factory_.GetWeakPtr()));
}

void LibassistantLoaderImpl::OnLibraryLoaded(
    base::ScopedNativeLibrary library) {
  if (!library.is_valid()) {
    DVLOG(1) << "Failed to load libassistant.so DLC,  error: "
             << library.GetError()->ToString();
    RecordLibassistantDlcLoadStatus(LoadStatus::kNotLoaded);
    RunCallback(/*success=*/false);
    return;
  }

  // Call exported function in libassistant.so.
  NewLibassistantEntrypointFn entrypoint =
      reinterpret_cast<NewLibassistantEntrypointFn>(
          library.GetFunctionPointer(kNewLibassistantEntrypointFnName));
  C_API_LibassistantEntrypoint* c_entrypoint = entrypoint(0);
  auto* entry_point =
      assistant_client::internal_api::LibassistantEntrypointFromC(c_entrypoint);

  DVLOG(3) << "Loaded libassistant.so.";
  RecordLibassistantDlcLoadStatus(LoadStatus::kLoaded);

  dlc_library_ = std::move(library);
  entry_point_ = base::WrapUnique(entry_point);
  RunCallback(/*success=*/true);
}

void LibassistantLoaderImpl::RunCallback(bool success) {
  std::move(callback_).Run(success);
}

// static
LibassistantLoaderImpl* LibassistantLoaderImpl::GetInstance() {
  // TODO(b/242098785): Investigate if we could remove NoDestructor.
  static base::NoDestructor<LibassistantLoaderImpl> instance;
  return instance.get();
}

// static
void LibassistantLoader::Load(LoadCallback callback) {
  LibassistantLoaderImpl::GetInstance()->Load(std::move(callback));
}

}  // namespace chromeos::libassistant
