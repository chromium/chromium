// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_KWALLET_H_
#define COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_KWALLET_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "components/os_crypt/sync/key_storage_linux.h"
#include "components/os_crypt/sync/kwallet_dbus.h"

class COMPONENT_EXPORT(OS_CRYPT) KeyStorageKWallet : public KeyStorageLinux {
 public:
  KeyStorageKWallet(base::nix::DesktopEnvironment desktop_env,
                    std::string app_name);

  KeyStorageKWallet(const KeyStorageKWallet&) = delete;
  KeyStorageKWallet& operator=(const KeyStorageKWallet&) = delete;

  ~KeyStorageKWallet() override;

  // Initialize using an optional KWalletDBus mock.
  // A DBus session will not be created if a mock is provided.
  bool InitWithKWalletDBus(std::unique_ptr<KWalletDBus> mock_kwallet_dbus_ptr);

 protected:
  // KeyStorageLinux
  bool Init() override;
  std::optional<std::string> GetKeyImpl() override;

 private:
  enum class InitResult {
    SUCCESS,
    TEMPORARY_FAIL,
    PERMANENT_FAIL,
  };

  static constexpr int kInvalidHandle = -1;

  // Check whether KWallet is enabled and set |wallet_name_|
  InitResult InitWallet();

  // Create Chrome's folder in the wallet, if it doesn't exist.
  bool InitFolder();

  // Generates a new 16-byte key, stores it in KWallet and returns the key
  // value.
  std::optional<std::string> GenerateAndStorePassword();

  const base::nix::DesktopEnvironment desktop_env_;
  int32_t handle_ = kInvalidHandle;
  std::string wallet_name_;
  const std::string app_name_;
  std::unique_ptr<KWalletDBus> kwallet_dbus_;
};

#endif  // COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_KWALLET_H_
