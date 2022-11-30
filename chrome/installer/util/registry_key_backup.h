// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_REGISTRY_KEY_BACKUP_H_
#define CHROME_INSTALLER_UTIL_REGISTRY_KEY_BACKUP_H_

#include <windows.h>

#include <memory>

// A container for a registry key, its values, and its subkeys.  We don't use
// more obvious methods for various reasons:
// - RegCopyTree isn't supported pre-Vista, so we'd have to do something
//   different for XP anyway.
// - SHCopyKey can't copy subkeys into a volatile destination, so we'd have to
//   worry about polluting the registry.
// We don't persist security attributes since we only delete keys that we own,
// and we don't set custom attributes on them anyway.
class RegistryKeyBackup {
 public:
  RegistryKeyBackup();

  RegistryKeyBackup(const RegistryKeyBackup&) = delete;
  RegistryKeyBackup& operator=(const RegistryKeyBackup&) = delete;

  ~RegistryKeyBackup();

  // Recursively reads |key_path| into this instance.  Backing up a non-existent
  // key is valid.  Returns true if the backup was successful; false otherwise,
  // in which case the state of this instance is not modified.
  bool Initialize(HKEY root, const wchar_t* key_path, REGSAM wow64_acccess);

  // Writes the contents of this instance into |key|.  The contents of
  // |key_path| are not modified If this instance is uninitialized or was
  // initialized from a non-existent key.
  bool WriteTo(HKEY root, const wchar_t* key_path, REGSAM wow64_acccess) const;

  void swap(RegistryKeyBackup& other) { key_data_.swap(other.key_data_); }

 private:
  class KeyData;

  // The values and subkeys of the backed-up key.
  std::unique_ptr<KeyData> key_data_;
};

#endif  // CHROME_INSTALLER_UTIL_REGISTRY_KEY_BACKUP_H_
