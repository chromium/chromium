// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_SYSTEM_SALT_GETTER_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_SYSTEM_SALT_GETTER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

namespace ash {

// This class is used to get the system salt from cryptohome and cache it.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) SystemSaltGetter {
 public:
  using GetSystemSaltCallback =
      base::OnceCallback<void(const std::string& system_salt)>;
  using RawSalt = std::vector<uint8_t>;

  // Manage an explicitly initialized global instance.
  static void Initialize();
  static bool IsInitialized();
  static void Shutdown();
  static SystemSaltGetter* Get();

  SystemSaltGetter(const SystemSaltGetter&) = delete;
  SystemSaltGetter& operator=(const SystemSaltGetter&) = delete;

  // Converts |salt| to a hex encoded string.
  static std::string ConvertRawSaltToHexString(const RawSalt& salt);

  // Returns system hash in hex encoded ascii format. Note: this may return
  // an empty string (e.g. errors in D-Bus layer)
  void GetSystemSalt(GetSystemSaltCallback callback);

  // Adds another callback to be called when system salt is received.
  // (If system salt is available, closure will be called immediately).
  void AddOnSystemSaltReady(base::OnceClosure closure);

  // Returns pointer to binary system salt if it is already known.
  // Returns nullptr if system salt is not known.
  // WARNING: This pointer is null early in startup. Do not assume it is valid.
  // Prefer GetSystemSalt() above. https://crbug.com/1122674
  const RawSalt* GetRawSalt() const;

  // This is for browser tests API.
  void SetRawSaltForTesting(const RawSalt& raw_salt);

 protected:
  SystemSaltGetter();
  ~SystemSaltGetter();

 private:
  // Used to implement GetSystemSalt().
  void DidWaitForServiceToBeAvailable(GetSystemSaltCallback callback,
                                      bool service_is_available);
  void DidGetSystemSalt(
      GetSystemSaltCallback system_salt_callback,
      std::optional<::user_data_auth::GetSystemSaltReply> system_salt_reply);

  RawSalt raw_salt_;
  std::string system_salt_;

  // List of callbacks waiting for system salt ready event.
  std::vector<base::OnceClosure> on_system_salt_ready_;

  base::WeakPtrFactory<SystemSaltGetter> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_SYSTEM_SALT_GETTER_H_
