// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KWALLET_DBUS_H_
#define COMPONENTS_OS_CRYPT_KWALLET_DBUS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/nix/xdg_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class Bus;
class ObjectProxy;
}

// Contains wrappers for dbus invocations related to KWallet.
class COMPONENT_EXPORT(OS_CRYPT) KWalletDBus {
 public:
  // Error code for dbus calls to kwallet.
  enum Error { SUCCESS = 0, CANNOT_CONTACT, CANNOT_READ };

  explicit KWalletDBus(base::nix::DesktopEnvironment desktop_env);

  KWalletDBus(const KWalletDBus&) = delete;
  KWalletDBus& operator=(const KWalletDBus&) = delete;

  virtual ~KWalletDBus();

  // Set the bus that we will use. Required before any other operation.
  // The owner of KWalletDBus is responsible for killing the bus.
  virtual void SetSessionBus(scoped_refptr<dbus::Bus> session_bus);

  // Expose the bus so that shutdown can be scheduled asynchronously.
  virtual dbus::Bus* GetSessionBus();

  // Use KLauncher to start the KWallet service. Returns true if successful.
  virtual bool StartKWalletd() WARN_UNUSED_RESULT;

  // The functions below are wrappers for calling the eponymous KWallet dbus
  // methods. They take pointers to locations where the return values will be
  // written. More KWallet documentation at
  // https://api.kde.org/4.12-api/kdelibs-apidocs/kdeui/html/classKWallet_1_1Wallet.html

  // Determine if the KDE wallet is enabled.
  virtual Error IsEnabled(bool* enabled) WARN_UNUSED_RESULT;

  // Get the name of the wallet used to store network passwords.
  virtual Error NetworkWallet(std::string* wallet_name_ptr) WARN_UNUSED_RESULT;

  // Open the |wallet_name| wallet for use.
  virtual Error Open(const std::string& wallet_name,
                     const std::string& app_name,
                     int* handle_ptr) WARN_UNUSED_RESULT;

  // Determine if the current folder has they entry key.
  virtual Error HasEntry(int wallet_handle,
                         const std::string& folder_name,
                         const std::string& signon_realm,
                         const std::string& app_name,
                         bool* has_entry_ptr) WARN_UNUSED_RESULT;

  // Read the entry key from the current folder.
  virtual Error ReadEntry(int wallet_handle,
                          const std::string& folder_name,
                          const std::string& signon_realm,
                          const std::string& app_name,
                          std::vector<uint8_t>* bytes_ptr) WARN_UNUSED_RESULT;

  // Return the list of keys of all entries in this folder.
  virtual Error EntryList(int wallet_handle,
                          const std::string& folder_name,
                          const std::string& app_name,
                          std::vector<std::string>* entry_list_ptr)
      WARN_UNUSED_RESULT;

  // Remove the entry key from the current folder.
  // |*return_code_ptr| is 0 on success.
  virtual Error RemoveEntry(int wallet_handle,
                            const std::string& folder_name,
                            const std::string& signon_realm,
                            const std::string& app_name,
                            int* return_code_ptr) WARN_UNUSED_RESULT;

  // Write a binary entry to the current folder.
  // |*return_code_ptr| is 0 on success.
  virtual Error WriteEntry(int wallet_handle,
                           const std::string& folder_name,
                           const std::string& signon_realm,
                           const std::string& app_name,
                           const uint8_t* data,
                           size_t length,
                           int* return_code_ptr) WARN_UNUSED_RESULT;

  // Determine if the folder |folder_name| exists in the wallet.
  virtual Error HasFolder(int handle,
                          const std::string& folder_name,
                          const std::string& app_name,
                          bool* has_folder_ptr) WARN_UNUSED_RESULT;

  // Create the folder |folder_name|.
  virtual Error CreateFolder(int handle,
                             const std::string& folder_name,
                             const std::string& app_name,
                             bool* success_ptr) WARN_UNUSED_RESULT;

  // Write a password to the current folder.
  virtual Error WritePassword(int handle,
                              const std::string& folder_name,
                              const std::string& key,
                              const std::string& password,
                              const std::string& app_name,
                              bool* write_success_ptr) WARN_UNUSED_RESULT;

  // Read the password for |key| from |folder_name|.
  // Clear |password_ptr| if no such password exists.
  virtual Error ReadPassword(int handle,
                             const std::string& folder_name,
                             const std::string& key,
                             const std::string& app_name,
                             absl::optional<std::string>* const password_ptr)
      WARN_UNUSED_RESULT;

  // Close the wallet. The wallet will only be closed if it is open but not in
  // use (rare), or if it is forced closed.
  virtual Error Close(int handle,
                      bool force,
                      const std::string& app_name,
                      bool* success_ptr) WARN_UNUSED_RESULT;

 private:
  // DBus handle for communication with klauncher and kwalletd.
  scoped_refptr<dbus::Bus> session_bus_;
  // Object proxy for kwalletd. We do not own this.
  dbus::ObjectProxy* kwallet_proxy_;

  // KWallet DBus name.
  std::string dbus_service_name_;
  // DBus path to KWallet interfaces.
  std::string dbus_path_;
  // The name used for logging and by klauncher when starting KWallet.
  std::string kwalletd_name_;
};

#endif  // COMPONENTS_OS_CRYPT_KWALLET_DBUS_H_
