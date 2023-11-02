// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_REGISTRY_ENTRY_H_
#define CHROME_INSTALLER_UTIL_REGISTRY_ENTRY_H_

#include <windows.h>

#include <stdint.h>

#include <string>

class WorkItemList;

// This class represents a single registry entry (a key and its value). A
// collection of registry entries should be collected into a list and written
// transactionally using a WorkItemList. This is preferred to writing to the
// registry directly, because if anything goes wrong, they can be rolled back.
//
// NOTE: This uses the default WOW64 view (32-bit on 32-bit applications, 64-bit
// on 64-bit applications). If the view needs to be customized, a parameter
// should be added, like in WorkItem. http://crbug.com/569816.
class RegistryEntry {
 public:
  // A bit-field enum of places to look for this key in the Windows registry.
  enum LookForIn {
    LOOK_IN_HKCU = 1 << 0,
    LOOK_IN_HKLM = 1 << 1,
    LOOK_IN_HKCU_THEN_HKLM = LOOK_IN_HKCU | LOOK_IN_HKLM,
  };

  // Identifies the type of removal this RegistryEntry is flagged for, if any.
  enum class RemovalFlag {
    // Default case: install the key/value.
    NONE,
    // Registry value under |key_path_|\|name_| is flagged for deletion.
    VALUE,
    // Registry key under |key_path_| is flag for deletion.
    KEY,
  };

  // Create an object that represent default value of a key.
  RegistryEntry(const std::wstring& key_path, const std::wstring& value);

  // Create an object that represent a key of type REG_SZ.
  RegistryEntry(const std::wstring& key_path,
                const std::wstring& name,
                const std::wstring& value);

  // Create an object that represent a key of integer type.
  RegistryEntry(const std::wstring& key_path,
                const std::wstring& name,
                DWORD value);

  RegistryEntry(const RegistryEntry&) = delete;
  RegistryEntry& operator=(const RegistryEntry&) = delete;

  // Flags this RegistryKey with |removal_flag|, indicating that it should be
  // removed rather than created. Note that this will not result in cleaning up
  // the entire registry hierarchy below RegistryEntry even if it is left empty
  // by this operation (this should thus not be used for uninstall, but only to
  // unregister keys that should explicitly no longer be active in the current
  // configuration).
  void set_removal_flag(RemovalFlag removal_flag) {
    removal_flag_ = removal_flag;
  }

  // Generates work_item tasks required to create (or potentially delete based
  // on |removal_flag_|) the current RegistryEntry and add them to the given
  // work item list.
  void AddToWorkItemList(HKEY root, WorkItemList* items) const;

  // Returns true if this key is flagged for removal.
  bool IsFlaggedForRemoval() const {
    return removal_flag_ != RemovalFlag::NONE;
  }

  // Checks if the current registry entry exists in HKCU\|key_path_|\|name_|
  // and value is |value_|. If the key does NOT exist in HKCU, checks for
  // the correct name and value in HKLM.
  // |look_for_in| specifies roots (HKCU and/or HKLM) in which to look for the
  // key, unspecified roots are not looked into (i.e. the the key is assumed not
  // to exist in them).
  // |look_for_in| must at least specify one root to look into.
  // If |look_for_in| is LOOK_IN_HKCU_THEN_HKLM, this method mimics Windows'
  // behavior when searching in HKCR (HKCU takes precedence over HKLM). For
  // registrations outside of HKCR on versions of Windows prior to Win8,
  // Chrome's values go in HKLM. This function will make unnecessary (but
  // harmless) queries into HKCU in that case.
  bool ExistsInRegistry(uint32_t look_for_in) const;

  // Checks if the current registry entry exists in \|key_path_|\|name_|,
  // regardless of value. Same lookup rules as ExistsInRegistry.
  // Unlike ExistsInRegistry, this returns true if some other value is present
  // with the same key.
  bool KeyExistsInRegistry(uint32_t look_for_in) const;

  const std::wstring& key_path() const { return key_path_; }

 private:
  // States this RegistryKey can be in compared to the registry.
  enum RegistryStatus {
    // |name_| does not exist in the registry
    DOES_NOT_EXIST,
    // |name_| exists, but its value != |value_|
    DIFFERENT_VALUE,
    // |name_| exists and its value is |value_|
    SAME_VALUE,
  };

  std::wstring key_path_;    // key path for the registry entry
  std::wstring name_;        // name of the registry entry
  bool is_string_;           // true if current registry entry is of type REG_SZ
  std::wstring value_;       // string value (useful if is_string_ = true)
  DWORD int_value_;          // integer value (useful if is_string_ = false)

  // Identifies whether this RegistryEntry is flagged for removal (i.e. no
  // longer relevant on the configuration it was created under).
  RemovalFlag removal_flag_;

  // Helper function for ExistsInRegistry().
  // Returns the RegistryStatus of the current registry entry in
  // |root|\|key_path_|\|name_|.
  RegistryStatus StatusInRegistryUnderRoot(HKEY root) const;
};

#endif  // CHROME_INSTALLER_UTIL_REGISTRY_ENTRY_H_
