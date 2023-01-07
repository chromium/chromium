// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_kwallet.h"

#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "components/os_crypt/kwallet_dbus.h"
#include "dbus/bus.h"

KeyStorageKWallet::KeyStorageKWallet(base::nix::DesktopEnvironment desktop_env,
                                     std::string app_name)
    : desktop_env_(desktop_env), app_name_(std::move(app_name)) {}

KeyStorageKWallet::~KeyStorageKWallet() {
  // The handle is shared between programs that are using the same wallet.
  // Closing the wallet is a nop in the typical case.
  bool success = true;
  std::ignore = kwallet_dbus_->Close(handle_, false, app_name_, &success);
  kwallet_dbus_->GetSessionBus()->ShutdownAndBlock();
}

bool KeyStorageKWallet::Init() {
  // Initialize using the production KWalletDBus.
  return InitWithKWalletDBus(nullptr);
}

bool KeyStorageKWallet::InitWithKWalletDBus(
    std::unique_ptr<KWalletDBus> mock_kwallet_dbus_ptr) {
  if (mock_kwallet_dbus_ptr) {
    kwallet_dbus_ = std::move(mock_kwallet_dbus_ptr);
  } else {
    // Initializing with production KWalletDBus
    kwallet_dbus_ = std::make_unique<KWalletDBus>(desktop_env_);
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    kwallet_dbus_->SetSessionBus(base::MakeRefCounted<dbus::Bus>(options));
  }

  InitResult result = InitWallet();
  // If KWallet might not have started, attempt to start it and retry.
  if (result == InitResult::TEMPORARY_FAIL && kwallet_dbus_->StartKWalletd())
    result = InitWallet();

  return result == InitResult::SUCCESS;
}

KeyStorageKWallet::InitResult KeyStorageKWallet::InitWallet() {
  // Check that KWallet is enabled.
  bool enabled = false;
  KWalletDBus::Error error = kwallet_dbus_->IsEnabled(&enabled);
  switch (error) {
    case KWalletDBus::Error::CANNOT_CONTACT:
      return InitResult::TEMPORARY_FAIL;
    case KWalletDBus::Error::CANNOT_READ:
      return InitResult::PERMANENT_FAIL;
    case KWalletDBus::Error::SUCCESS:
      break;
  }
  if (!enabled)
    return InitResult::PERMANENT_FAIL;

  // Get the wallet name.
  error = kwallet_dbus_->NetworkWallet(&wallet_name_);
  switch (error) {
    case KWalletDBus::Error::CANNOT_CONTACT:
      return InitResult::TEMPORARY_FAIL;
    case KWalletDBus::Error::CANNOT_READ:
      return InitResult::PERMANENT_FAIL;
    case KWalletDBus::Error::SUCCESS:
      return InitResult::SUCCESS;
  }

  NOTREACHED();
  return InitResult::PERMANENT_FAIL;
}

absl::optional<std::string> KeyStorageKWallet::GetKeyImpl() {
  // Get handle
  KWalletDBus::Error error =
      kwallet_dbus_->Open(wallet_name_, app_name_, &handle_);
  if (error || handle_ == kInvalidHandle)
    return absl::nullopt;

  // Create folder
  if (!InitFolder())
    return absl::nullopt;

  // Read password
  absl::optional<std::string> password;
  error =
      kwallet_dbus_->ReadPassword(handle_, KeyStorageLinux::kFolderName,
                                  KeyStorageLinux::kKey, app_name_, &password);
  if (error)
    return absl::nullopt;

  // If there is no entry, generate and write a new password.
  if (!password.has_value()) {
    std::string password_;
    base::Base64Encode(base::RandBytesAsString(16), &password_);
    bool success;
    error = kwallet_dbus_->WritePassword(handle_, KeyStorageLinux::kFolderName,
                                         KeyStorageLinux::kKey, password_,
                                         app_name_, &success);
    if (error || !success)
      return absl::nullopt;
    password = std::move(password_);
  }

  return password;
}

bool KeyStorageKWallet::InitFolder() {
  bool has_folder = false;
  KWalletDBus::Error error = kwallet_dbus_->HasFolder(
      handle_, KeyStorageLinux::kFolderName, app_name_, &has_folder);
  if (error)
    return false;

  if (!has_folder) {
    bool success = false;
    error = kwallet_dbus_->CreateFolder(handle_, KeyStorageLinux::kFolderName,
                                        app_name_, &success);
    if (error || !success)
      return false;
  }

  return true;
}
