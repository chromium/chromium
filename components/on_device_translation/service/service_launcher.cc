// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service/service_launcher.h"

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/service/file_operation_proxy_impl.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace on_device_translation {
namespace {

using mojom::FileOperationProxy;
using mojom::OnDeviceTranslationServiceConfig;

// Prefix for the display name of the on-device translation service. The origin
// is appended to the prefix.
constexpr std::string_view kOnDeviceTranslationServiceDisplayNamePrefix =
    "On-device Translation Service: ";

std::string ToString(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/362123222): Get rid of conditional decoding.
  return path.AsUTF8Unsafe();
#else
  return path.value();
#endif  // BUILDFLAG(IS_WIN)
}

std::vector<base::FilePath> GetLanguagePackInfo(
    std::vector<mojom::OnDeviceTranslationLanguagePackagePtr>& packages) {
  CHECK(packages.empty());
  std::vector<base::FilePath> package_paths;
  for (const auto& it : kLanguagePackComponentConfigMap) {
    auto file_path =
        OnDeviceTranslationInstaller::GetInstance()->GetLanguagePackPath(
            it.first);
    if (!file_path.empty()) {
      packages.push_back(mojom::OnDeviceTranslationLanguagePackage::New(
          std::string(ToLanguageCode(it.second->language1)),
          std::string(ToLanguageCode(it.second->language2))));
      package_paths.push_back(file_path);
    }
  }

  return package_paths;
}
}  // namespace

class OnDeviceTranslationServiceLauncherImpl
    : public OnDeviceTranslationServiceLauncher {
 public:
  OnDeviceTranslationServiceLauncherImpl() = default;
  ~OnDeviceTranslationServiceLauncherImpl() override = default;

  OnDeviceTranslationServiceLauncherImpl(
      const OnDeviceTranslationServiceLauncherImpl&) = delete;
  OnDeviceTranslationServiceLauncherImpl& operator=(
      const OnDeviceTranslationServiceLauncherImpl&) = delete;

  mojo::PendingRemote<mojom::OnDeviceTranslationService> Launch(
      std::string_view service_display_name_suffix) override {
    mojo::PendingRemote<mojom::OnDeviceTranslationService> remote;
    auto receiver = remote.InitWithNewPipeAndPassReceiver();

    const base::FilePath binary_path =
        OnDeviceTranslationInstaller::GetInstance()->GetLibraryPath();
    CHECK(!binary_path.empty())
        << "Got an empty path to TranslateKit binary on the device.";

    std::vector<std::string> extra_switches;
    extra_switches.push_back(
        base::StrCat({kTranslateKitBinaryPath, "=", ToString(binary_path)}));

    content::ServiceProcessHost::Launch<mojom::OnDeviceTranslationService>(
        std::move(receiver),
        content::ServiceProcessHost::Options()
            .WithDisplayName(
                base::StrCat({kOnDeviceTranslationServiceDisplayNamePrefix,
                              service_display_name_suffix}))
            .WithExtraCommandLineSwitches(extra_switches)
#if BUILDFLAG(IS_WIN)
            .WithPreloadedLibraries(
                {binary_path},
                content::ServiceProcessHostPreloadLibraries::GetPassKey())
#endif
            .Pass());

    auto config = OnDeviceTranslationServiceConfig::New();
    mojo::PendingReceiver<FileOperationProxy> proxy_receiver =
        config->file_operation_proxy.InitWithNewPipeAndPassReceiver();
    mojo::Remote<mojom::OnDeviceTranslationService> service_remote(
        std::move(remote));
    std::vector<base::FilePath> package_paths =
        GetLanguagePackInfo(config->packages);
    service_remote->SetServiceConfig(std::move(config));

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    // Create the FileOperationProxy which lives in the background thread of
    // `task_runner`.
    // Bind the self-owned receiver on the background thread.
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::PendingReceiver<mojom::FileOperationProxy> receiver,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               std::vector<base::FilePath> package_paths) {
              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<FileOperationProxyImpl>(
                      // Note: Ensure the constructor doesn't also bind the
                      // receiver!
                      task_runner, std::move(package_paths)),
                  std::move(receiver));
            },
            std::move(proxy_receiver), task_runner, std::move(package_paths)));

    return service_remote.Unbind();
  }
};

std::unique_ptr<OnDeviceTranslationServiceLauncher>
CreateOnDeviceTranslationServiceLauncher() {
  return std::make_unique<OnDeviceTranslationServiceLauncherImpl>();
}

}  // namespace on_device_translation
