// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_linux.h"

#include "base/bind.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "components/os_crypt/key_storage_util_linux.h"

#if defined(USE_LIBSECRET)
#include "components/os_crypt/key_storage_libsecret.h"
#endif
#if defined(USE_KEYRING)
#include "components/os_crypt/key_storage_keyring.h"
#endif
#if defined(USE_KWALLET)
#include "components/os_crypt/key_storage_kwallet.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char KeyStorageLinux::kFolderName[] = "Chrome Keys";
const char KeyStorageLinux::kKey[] = "Chrome Safe Storage";
#else
const char KeyStorageLinux::kFolderName[] = "Chromium Keys";
const char KeyStorageLinux::kKey[] = "Chromium Safe Storage";
#endif

#if defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)

namespace {

const char* SelectedLinuxBackendToString(
    os_crypt::SelectedLinuxBackend selection) {
  switch (selection) {
    case os_crypt::SelectedLinuxBackend::DEFER:
      return "DEFER";
    case os_crypt::SelectedLinuxBackend::BASIC_TEXT:
      return "BASIC_TEXT";
    case os_crypt::SelectedLinuxBackend::GNOME_ANY:
      return "GNOME_ANY";
    case os_crypt::SelectedLinuxBackend::GNOME_KEYRING:
      return "GNOME_KEYRING";
    case os_crypt::SelectedLinuxBackend::GNOME_LIBSECRET:
      return "GNOME_LIBSECRET";
    case os_crypt::SelectedLinuxBackend::KWALLET:
      return "KWALLET";
    case os_crypt::SelectedLinuxBackend::KWALLET5:
      return "KWALLET5";
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

#endif  // defined(USE_LIBSECRET) || defined(USE_KEYRING) ||
        // defined(USE_KWALLET)

// static
std::unique_ptr<KeyStorageLinux> KeyStorageLinux::CreateService(
    const os_crypt::Config& config) {
#if defined(USE_LIBSECRET) || defined(USE_KEYRING) || defined(USE_KWALLET)
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

  // TODO(crbug.com/782851) Schedule the initialisation on each backend's
  // favourite thread.

  // Try initializing the selected backend.
  // In case of GNOME_ANY, prefer Libsecret
  std::unique_ptr<KeyStorageLinux> key_storage;

#if defined(USE_LIBSECRET)
  if (selected_backend == os_crypt::SelectedLinuxBackend::GNOME_ANY ||
      selected_backend == os_crypt::SelectedLinuxBackend::GNOME_LIBSECRET) {
    key_storage.reset(new KeyStorageLibsecret());
    if (key_storage->WaitForInitOnTaskRunner()) {
      VLOG(1) << "OSCrypt using Libsecret as backend.";
      return key_storage;
    }
    LOG(WARNING) << "OSCrypt tried Libsecret but couldn't initialise.";
  }
#endif  // defined(USE_LIBSECRET)

#if defined(USE_KEYRING)
  if (selected_backend == os_crypt::SelectedLinuxBackend::GNOME_ANY ||
      selected_backend == os_crypt::SelectedLinuxBackend::GNOME_KEYRING) {
    key_storage.reset(new KeyStorageKeyring(config.main_thread_runner));
    if (key_storage->WaitForInitOnTaskRunner()) {
      VLOG(1) << "OSCrypt using Keyring as backend.";
      return key_storage;
    }
    LOG(WARNING) << "OSCrypt tried Keyring but couldn't initialise.";
  }
#endif  // defined(USE_KEYRING)

#if defined(USE_KWALLET)
  if (selected_backend == os_crypt::SelectedLinuxBackend::KWALLET ||
      selected_backend == os_crypt::SelectedLinuxBackend::KWALLET5) {
    DCHECK(!config.product_name.empty());
    base::nix::DesktopEnvironment used_desktop_env =
        selected_backend == os_crypt::SelectedLinuxBackend::KWALLET
            ? base::nix::DESKTOP_ENVIRONMENT_KDE4
            : base::nix::DESKTOP_ENVIRONMENT_KDE5;
    key_storage.reset(
        new KeyStorageKWallet(used_desktop_env, config.product_name));
    if (key_storage->WaitForInitOnTaskRunner()) {
      VLOG(1) << "OSCrypt using KWallet as backend.";
      return key_storage;
    }
    LOG(WARNING) << "OSCrypt tried KWallet but couldn't initialise.";
  }
#endif  // defined(USE_KWALLET)
#endif  // defined(USE_LIBSECRET) || defined(USE_KEYRING) ||
        // defined(USE_KWALLET)

  // Either there are no supported backends on this platform, or we chose to
  // use no backend, or the chosen backend failed to initialise.
  VLOG(1) << "OSCrypt did not initialize a backend.";
  return nullptr;
}

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

std::string KeyStorageLinux::GetKey() {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_sync_primitives;
  base::SequencedTaskRunner* task_runner = GetTaskRunner();

  // We don't need to change threads if the backend has no preference or if we
  // are already on the right thread.
  if (!task_runner || task_runner->RunsTasksInCurrentSequence())
    return GetKeyImpl();

  base::WaitableEvent password_loaded(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::string password;
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
    std::string* password) {
  *password = GetKeyImpl();
  on_password_received->Signal();
}

void KeyStorageLinux::BlockOnInitThenSignal(base::WaitableEvent* on_inited,
                                            bool* success) {
  *success = Init();
  on_inited->Signal();
}
