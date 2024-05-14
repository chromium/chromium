// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/key_storage_linux.h"

#include <memory>

#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "components/os_crypt/sync/key_storage_config_linux.h"

#if defined(USE_LIBSECRET)
#include "components/os_crypt/sync/key_storage_libsecret.h"
#endif
#if defined(USE_KWALLET)
#include "components/os_crypt/sync/key_storage_kwallet.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char KeyStorageLinux::kFolderName[] = "Chrome Keys";
const char KeyStorageLinux::kKey[] = "Chrome Safe Storage";
#else
const char KeyStorageLinux::kFolderName[] = "Chromium Keys";
const char KeyStorageLinux::kKey[] = "Chromium Safe Storage";
#endif

namespace {

// Used for metrics. Do not rearrange.
enum class BackendUsage {
  // A backend was selected and used.
  // *_FAILED means the backend was selected but couldn't be used.
  kDefer = 0,
  kDeferFailed = 1,
  kBasicText = 2,
  kBasicTextFailed = 3,
  // gnome-keyring support has been dropped, but the enum slots corresponding
  // to it should not be used since this enum is also used for metrics.
  // kGnomeAny = 4,
  // kGnomeAnyFailed = 5,
  // kGnomeKeyring = 6,
  // kGnomeKeyringFailed = 7,
  kGnomeLibsecret = 8,
  kGnomeLibsecretFailed = 9,
  kKwallet = 10,
  kKwalletFailed = 11,
  kKwallet5 = 12,
  kKwallet5Failed = 13,
  kKwallet6 = 14,
  kKwallet6Failed = 15,
  kMaxValue = kKwallet6Failed,
};

constexpr BackendUsage SelectedBackendToMetric(
    os_crypt::SelectedLinuxBackend selection,
    bool used) {
  switch (selection) {
    case os_crypt::SelectedLinuxBackend::DEFER:
      return used ? BackendUsage::kDefer : BackendUsage::kDeferFailed;
    case os_crypt::SelectedLinuxBackend::BASIC_TEXT:
      return used ? BackendUsage::kBasicText : BackendUsage::kBasicTextFailed;
    case os_crypt::SelectedLinuxBackend::GNOME_LIBSECRET:
      return used ? BackendUsage::kGnomeLibsecret
                  : BackendUsage::kGnomeLibsecretFailed;
    case os_crypt::SelectedLinuxBackend::KWALLET:
      return used ? BackendUsage::kKwallet : BackendUsage::kKwalletFailed;
    case os_crypt::SelectedLinuxBackend::KWALLET5:
      return used ? BackendUsage::kKwallet5 : BackendUsage::kKwallet5Failed;
    case os_crypt::SelectedLinuxBackend::KWALLET6:
      return used ? BackendUsage::kKwallet6 : BackendUsage::kKwallet6Failed;
  }
  NOTREACHED_IN_MIGRATION();
  return BackendUsage::kDeferFailed;
}

const char* SelectedLinuxBackendToString(
    os_crypt::SelectedLinuxBackend selection) {
  switch (selection) {
    case os_crypt::SelectedLinuxBackend::DEFER:
      return "DEFER";
    case os_crypt::SelectedLinuxBackend::BASIC_TEXT:
      return "BASIC_TEXT";
    case os_crypt::SelectedLinuxBackend::GNOME_LIBSECRET:
      return "GNOME_LIBSECRET";
    case os_crypt::SelectedLinuxBackend::KWALLET:
      return "KWALLET";
    case os_crypt::SelectedLinuxBackend::KWALLET5:
      return "KWALLET5";
    case os_crypt::SelectedLinuxBackend::KWALLET6:
      return "KWALLET6";
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace

// static
std::unique_ptr<KeyStorageLinux> KeyStorageLinux::CreateService(
    const os_crypt::Config& config) {
  // Select a backend.
  bool use_backend = !config.should_use_preference ||
                     os_crypt::GetBackendUse(config.user_data_path);
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop_env =
      base::nix::GetDesktopEnvironment(env.get());
  os_crypt::SelectedLinuxBackend selected_backend =
      os_crypt::SelectBackend(config.store, use_backend, desktop_env);
  VLOG(1) << "Selected backend for OSCrypt: "
          << SelectedLinuxBackendToString(selected_backend);

  // TODO(crbug.com/40548841) Schedule the initialisation on each backend's
  // favourite thread.

  // Try initializing the selected backend.
  // In case of GNOME_ANY, prefer Libsecret
  std::unique_ptr<KeyStorageLinux> key_storage;
#if defined(USE_LIBSECRET) || defined(USE_KWALLET)
  key_storage = CreateServiceInternal(selected_backend, config);
#endif  // defined(USE_LIBSECRET) || defined(USE_KWALLET)

  UMA_HISTOGRAM_ENUMERATION(
      "OSCrypt.BackendUsage",
      SelectedBackendToMetric(selected_backend, key_storage != nullptr));

  // Either there are no supported backends on this platform, or we chose to
  // use no backend, or the chosen backend failed to initialise.
  VLOG_IF(1, !key_storage) << "OSCrypt did not initialize a backend.";
  return key_storage;
}

#if defined(USE_LIBSECRET) || defined(USE_KWALLET)
std::unique_ptr<KeyStorageLinux> KeyStorageLinux::CreateServiceInternal(
    os_crypt::SelectedLinuxBackend selected_backend,
    const os_crypt::Config& config) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static const base::NoDestructor<std::string> kDefaultApplicationName("chrome");
#else
  static const base::NoDestructor<std::string> kDefaultApplicationName("chromium");
#endif

  std::unique_ptr<KeyStorageLinux> key_storage;

#if defined(USE_LIBSECRET)
  if (selected_backend == os_crypt::SelectedLinuxBackend::GNOME_LIBSECRET) {
#if defined(ALLOW_RUNTIME_CONFIGURABLE_KEY_STORAGE)
    std::string application_name = config.application_name;
    if (application_name.empty()) {
      application_name = *kDefaultApplicationName;
    }
#else
    std::string application_name = *kDefaultApplicationName;
#endif
    key_storage = std::make_unique<KeyStorageLibsecret>(application_name);
    if (key_storage->WaitForInitOnTaskRunner()) {
      VLOG(1) << "OSCrypt using Libsecret as backend.";
      return key_storage;
    }
    LOG(WARNING) << "OSCrypt tried Libsecret but couldn't initialise.";
  }
#endif  // defined(USE_LIBSECRET)

#if defined(USE_KWALLET)
  if (selected_backend == os_crypt::SelectedLinuxBackend::KWALLET ||
      selected_backend == os_crypt::SelectedLinuxBackend::KWALLET5 ||
      selected_backend == os_crypt::SelectedLinuxBackend::KWALLET6) {
    DCHECK(!config.product_name.empty());
    base::nix::DesktopEnvironment used_desktop_env =
        base::nix::DESKTOP_ENVIRONMENT_KDE4;
    if (selected_backend == os_crypt::SelectedLinuxBackend::KWALLET5) {
      used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE5;
    }
    if (selected_backend == os_crypt::SelectedLinuxBackend::KWALLET6) {
      used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE6;
    }
    key_storage = std::make_unique<KeyStorageKWallet>(used_desktop_env,
                                                      config.product_name);
    if (key_storage->WaitForInitOnTaskRunner()) {
      VLOG(1) << "OSCrypt using KWallet as backend.";
      return key_storage;
    }
    LOG(WARNING) << "OSCrypt tried KWallet but couldn't initialise.";
  }
#endif  // defined(USE_KWALLET)

  return nullptr;
}
#endif  // defined(USE_LIBSECRET) || defined(USE_KWALLET)

bool KeyStorageLinux::WaitForInitOnTaskRunner() {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_sync_primitives;
  base::SequencedTaskRunner* task_runner = GetTaskRunner();

  // We don't need to change threads if the backend has no preference or if we
  // are already on the right thread.
  if (!task_runner || task_runner->RunsTasksInCurrentSequence())
    return Init();

  base::WaitableEvent initialized(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool success;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&KeyStorageLinux::BlockOnInitThenSignal,
                     base::Unretained(this), &initialized, &success));
  initialized.Wait();
  return success;
}

std::optional<std::string> KeyStorageLinux::GetKey() {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_sync_primitives;
  base::SequencedTaskRunner* task_runner = GetTaskRunner();

  // We don't need to change threads if the backend has no preference or if we
  // are already on the right thread.
  if (!task_runner || task_runner->RunsTasksInCurrentSequence())
    return GetKeyImpl();

  base::WaitableEvent password_loaded(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::optional<std::string> password;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&KeyStorageLinux::BlockOnGetKeyImplThenSignal,
                     base::Unretained(this), &password_loaded, &password));
  password_loaded.Wait();
  return password;
}

base::SequencedTaskRunner* KeyStorageLinux::GetTaskRunner() {
  return nullptr;
}

void KeyStorageLinux::BlockOnGetKeyImplThenSignal(
    base::WaitableEvent* on_password_received,
    std::optional<std::string>* password) {
  *password = GetKeyImpl();
  on_password_received->Signal();
}

void KeyStorageLinux::BlockOnInitThenSignal(base::WaitableEvent* on_inited,
                                            bool* success) {
  *success = Init();
  on_inited->Signal();
}
