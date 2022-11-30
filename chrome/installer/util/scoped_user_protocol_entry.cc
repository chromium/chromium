// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/scoped_user_protocol_entry.h"

#include <string>

#include "base/files/file_path.h"
#include "base/win/registry.h"
#include "chrome/installer/util/registry_entry.h"
#include "chrome/installer/util/shell_util.h"

ScopedUserProtocolEntry::ScopedUserProtocolEntry(const wchar_t* protocol) {
  entries_.push_back(std::make_unique<RegistryEntry>(
      base::FilePath(ShellUtil::kRegClasses).Append(protocol).value(),
      ShellUtil::kRegUrlProtocol, std::wstring()));
  if (!entries_.back()->KeyExistsInRegistry(RegistryEntry::LOOK_IN_HKCU) &&
      ShellUtil::AddRegistryEntries(HKEY_CURRENT_USER, entries_)) {
    return;
  }
  entries_.clear();
}

ScopedUserProtocolEntry::~ScopedUserProtocolEntry() {
  // The empty key is deleted only if it's created by ctor().
  if (entries_.empty())
    return;

  // The value hasn't been changed.
  if (!entries_.back()->ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU))
    return;

  // Key is still valid and only contains one value.
  base::win::RegKey key(HKEY_CURRENT_USER, entries_.back()->key_path().c_str(),
                        KEY_READ);
  if (!key.Valid() || key.GetValueCount() > 1)
    return;
  key.Close();

  // There is no subkey.
  if (base::win::RegistryKeyIterator(HKEY_CURRENT_USER,
                                     entries_.back()->key_path().c_str())
          .SubkeyCount() > 0) {
    return;
  }

  entries_.back()->set_removal_flag(RegistryEntry::RemovalFlag::KEY);
  ShellUtil::AddRegistryEntries(HKEY_CURRENT_USER, entries_);
}
