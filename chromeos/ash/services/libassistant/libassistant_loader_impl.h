// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_LOADER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_LOADER_IMPL_H_

#include <optional>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/services/libassistant/public/cpp/libassistant_loader.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "net/base/backoff_entry.h"

namespace ash::libassistant {

using EntryPoint = assistant_client::internal_api::LibassistantEntrypoint;

inline constexpr char kNewLibassistantEntrypointFnName[] =
    "NewLibassistantEntrypoint";

typedef C_API_LibassistantEntrypoint* (*NewLibassistantEntrypointFn)(
    int version);

// Implementation of `LibassistantLoader` to load libassistant.so for different
// situations.
class COMPONENT_EXPORT(LIBASSISTANT_LOADER) LibassistantLoaderImpl
    : public LibassistantLoader {
 public:
  static LibassistantLoaderImpl* GetInstance();

  LibassistantLoaderImpl(const LibassistantLoaderImpl&) = delete;
  LibassistantLoaderImpl& operator=(const LibassistantLoaderImpl&) = delete;

  // Load libassistant.so. Will skip if the library has been loaded.
  void Load(LoadCallback callback);

  // Load libassistant.so for sandbox. This is a blocking method.
  void LoadBlocking(const std::string& root_path);

  // Return the LibassistantEntrypoint.
  EntryPoint* GetEntryPoint();

 private:
  friend class base::NoDestructor<LibassistantLoaderImpl>;

  LibassistantLoaderImpl();
  ~LibassistantLoaderImpl() override;

  // Call DlcServiceClient to install libassistant DLC.
  void InstallDlc(LoadCallback callback);
  void OnInstallDlcComplete(const DlcserviceClient::InstallResult& result);

  // Called when the libassistant DLC is loaded.
  void OnLibraryLoaded(base::ScopedNativeLibrary library);

  void RunCallback(bool success);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  LoadCallback callback_;

  std::optional<base::ScopedNativeLibrary> dlc_library_;
  std::unique_ptr<EntryPoint> entry_point_;

  base::WeakPtrFactory<LibassistantLoaderImpl> weak_factory_{this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_LOADER_IMPL_H_
