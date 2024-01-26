// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_LINUX_H_
#define COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_LINUX_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "components/os_crypt/sync/key_storage_util_linux.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace os_crypt {
struct Config;
}

// An API for retrieving OSCrypt's password from the system's password storage
// service.
class COMPONENT_EXPORT(OS_CRYPT) KeyStorageLinux {
 public:
  KeyStorageLinux() = default;

  KeyStorageLinux(const KeyStorageLinux&) = delete;
  KeyStorageLinux& operator=(const KeyStorageLinux&) = delete;

  virtual ~KeyStorageLinux() = default;

  // Tries to load the appropriate key storage. Returns null if none succeed.
  static COMPONENT_EXPORT(OS_CRYPT)
      std::unique_ptr<KeyStorageLinux> CreateService(
          const os_crypt::Config& config);

  // Gets the encryption key from the OS password-managing library. If a key is
  // not found, a new key will be generated, stored and returned.
  std::optional<std::string> GetKey();

 protected:
  // Get the backend's favourite task runner, or nullptr for no preference.
  virtual base::SequencedTaskRunner* GetTaskRunner();

  // Loads the key storage. Returns false if the service is not available.
  // This iwill be called on the backend's preferred thread.
  virtual bool Init() = 0;

  // The implementation of GetKey() for a specific backend. This will be called
  // on the backend's preferred thread.
  virtual std::optional<std::string> GetKeyImpl() = 0;

  // The name of the group, if any, containing the key.
  static const char kFolderName[];
  // The name of the entry with the encryption key.
  static const char kKey[];

 private:
#if defined(USE_LIBSECRET) || defined(USE_KWALLET)
  // Tries to load the appropriate key storage. Returns null if none succeed.
  static std::unique_ptr<KeyStorageLinux> CreateServiceInternal(
      os_crypt::SelectedLinuxBackend selected_backend,
      const os_crypt::Config& config);
#endif  // defined(USE_LIBSECRET) || defined(USE_KWALLET)

  // Performs Init() on the backend's preferred thread.
  bool WaitForInitOnTaskRunner();

  // Perform the blocking calls to the backend to get the Key. Store it in
  // |password| and signal completion on |on_password_received|.
  void BlockOnGetKeyImplThenSignal(base::WaitableEvent* on_password_received,
                                   std::optional<std::string>* password);

  // Perform the blocking calls to the backend to initialise. Store the
  // initialisation result in |success| and signal completion on |on_inited|.
  void BlockOnInitThenSignal(base::WaitableEvent* on_inited, bool* success);
};

#endif  // COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_LINUX_H_
