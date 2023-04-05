// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_

#include <string>

#include "base/types/strong_alias.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

// Represents a Web Authentication passkey credential to be displayed in an
// autofill selection context.
class PasskeyCredential {
 public:
  using Username =
      base::StrongAlias<struct UsernameTag, absl::optional<std::string>>;
  using DeviceName = base::StrongAlias<struct DeviceNameTag, std::u16string>;
  using BackendId = base::StrongAlias<struct BackendIdTag, std::string>;

  PasskeyCredential(const Username& username,
                    const DeviceName& device_name,
                    const BackendId& backend_id);
  ~PasskeyCredential();

  PasskeyCredential(const PasskeyCredential&);
  PasskeyCredential& operator=(const PasskeyCredential&);

  PasskeyCredential(PasskeyCredential&&);
  PasskeyCredential& operator=(PasskeyCredential&&);

  const std::u16string& username() const { return username_; }
  const std::u16string& device_name() const { return device_name_; }
  const std::string& id() const { return backend_id_; }

 private:
  friend bool operator==(const PasskeyCredential& lhs,
                         const PasskeyCredential& rhs);

  std::u16string username_;
  std::u16string device_name_;
  std::string backend_id_;
};

bool operator==(const PasskeyCredential& lhs, const PasskeyCredential& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_
