// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_LIBSECRET_H_
#define COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_LIBSECRET_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "components/os_crypt/sync/key_storage_linux.h"

// Specialisation of KeyStorageLinux that uses Libsecret.
class COMPONENT_EXPORT(OS_CRYPT) KeyStorageLibsecret : public KeyStorageLinux {
 public:
  explicit KeyStorageLibsecret(std::string application_name);

  KeyStorageLibsecret(const KeyStorageLibsecret&) = delete;
  KeyStorageLibsecret& operator=(const KeyStorageLibsecret&) = delete;

  ~KeyStorageLibsecret() override = default;

 protected:
  // KeyStorageLinux
  bool Init() override;
  std::optional<std::string> GetKeyImpl() override;

 private:
  std::optional<std::string> AddRandomPasswordInLibsecret();

  const std::string application_name_;
};

#endif  // COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_LIBSECRET_H_
