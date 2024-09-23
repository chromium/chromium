// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_KWALLET_DBUS_H_
#define COMPONENTS_OS_CRYPT_SYNC_KWALLET_DBUS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"

namespace dbus {
class Bus;
class ObjectProxy;
}  // namespace dbus

// Contains wrappers for dbus invocations related to KWallet.
class COMPONENT_EXPORT(OS_CRYPT) KWalletDBus {
 public:
  // Error code for dbus calls to kwallet.
  enum Error { SUCCESS = 0, CANNOT_CONTACT, CANNOT_READ };

  // The type of a KWallet entry. See available types in
  // https://github.com/KDE/kwallet/blob/master/src/api/KWallet/kwallet.h.
  enum class Type {
    kUnknown = 0,
    kPassword,
    kStream,
    kMap,
    kMaxValue = kMap,
  };

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
  [[nodiscard]] virtual bool StartKWalletd();

  // The functions below are wrappers for calling the eponymous KWallet dbus
  // methods. They take pointers to locations where the return values will be
  // written. More KWallet documentation at
  // https://api.kde.org/4.12-api/kdelibs-apidocs/kdeui/html/classKWallet_1_1Wallet.html

  // Determine if the KDE wallet is enabled.
  [[nodiscard]] virtual Error IsEnabled(bool* enabled);

  // Get the name of the wallet used to store network passwords.
  [[nodiscard]] virtual Error NetworkWallet(std::string* wallet_name_ptr);

  // Open the |wallet_name| wallet for use.
  [[nodiscard]] virtual Error Open(const std::string& wallet_name,
                                   const std::string& app_name,
                                   int* handle_ptr);

  // Determine if the current folder has the entry key.
  [[nodiscard]] virtual Error HasEntry(int wallet_handle,
                                       const std::string& folder_name,
                                       const std::string& key,
                                       const std::string& app_name,
                                       bool* has_entry_ptr);

  // Get the type of the value of an entry.
  [[nodiscard]] virtual Error EntryType(int wallet_handle,
                                        const std::string& folder_name,
                                        const std::string& key,
                                        const std::string& app_name,
                                        Type* entry_type_ptr);

  // Remove the entry key from the current folder.
  // |*return_code_ptr| is 0 on success.
  [[nodiscard]] virtual Error RemoveEntry(int wallet_handle,
                                          const std::string& folder_name,
                                          const std::string& key,
                                          const std::string& app_name,
                                          int* return_code_ptr);

  // Determine if the folder |folder_name| exists in the wallet.
  [[nodiscard]] virtual Error HasFolder(int handle,
                                        const std::string& folder_name,
                                        const std::string& app_name,
                                        bool* has_folder_ptr);

  // Create the folder |folder_name|.
  [[nodiscard]] virtual Error CreateFolder(int handle,
                                           const std::string& folder_name,
                                           const std::string& app_name,
                                           bool* success_ptr);

  // Write a password to the current folder.
  [[nodiscard]] virtual Error WritePassword(int handle,
                                            const std::string& folder_name,
                                            const std::string& key,
                                            const std::string& password,
                                            const std::string& app_name,
                                            bool* write_success_ptr);

  // Read the password for |key| from |folder_name|.
  // Clear |password_ptr| if no such password exists.
  [[nodiscard]] virtual Error ReadPassword(
      int handle,
      const std::string& folder_name,
      const std::string& key,
      const std::string& app_name,
      std::optional<std::string>* const password_ptr);

  // Close the wallet. The wallet will only be closed if it is open but not in
  // use (rare), or if it is forced closed.
  [[nodiscard]] virtual Error Close(int handle,
                                    bool force,
                                    const std::string& app_name,
                                    bool* success_ptr);

 private:
  // DBus handle for communication with klauncher and kwalletd.
  scoped_refptr<dbus::Bus> session_bus_;
  // Object proxy for kwalletd.
  scoped_refptr<dbus::ObjectProxy> kwallet_proxy_;

  // KWallet DBus name.
  std::string dbus_service_name_;
  // DBus path to KWallet interfaces.
  std::string dbus_path_;
  // The name used for logging and by klauncher when starting KWallet.
  std::string kwalletd_name_;
};

#endif  // COMPONENTS_OS_CRYPT_SYNC_KWALLET_DBUS_H_
