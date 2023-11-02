// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/registry_entry.h"

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

RegistryEntry::RegistryEntry(const std::wstring& key_path,
                             const std::wstring& value)
    : key_path_(key_path),
      name_(),
      is_string_(true),
      value_(value),
      int_value_(0),
      removal_flag_(RemovalFlag::NONE) {}

RegistryEntry::RegistryEntry(const std::wstring& key_path,
                             const std::wstring& name,
                             const std::wstring& value)
    : key_path_(key_path),
      name_(name),
      is_string_(true),
      value_(value),
      int_value_(0),
      removal_flag_(RemovalFlag::NONE) {}

RegistryEntry::RegistryEntry(const std::wstring& key_path,
                             const std::wstring& name,
                             DWORD value)
    : key_path_(key_path),
      name_(name),
      is_string_(false),
      value_(),
      int_value_(value),
      removal_flag_(RemovalFlag::NONE) {}

void RegistryEntry::AddToWorkItemList(HKEY root, WorkItemList* items) const {
  if (removal_flag_ == RemovalFlag::VALUE) {
    items->AddDeleteRegValueWorkItem(root, key_path_, WorkItem::kWow64Default,
                                     name_);
  } else if (removal_flag_ == RemovalFlag::KEY) {
    items->AddDeleteRegKeyWorkItem(root, key_path_, WorkItem::kWow64Default);
  } else {
    DCHECK(removal_flag_ == RemovalFlag::NONE);
    items->AddCreateRegKeyWorkItem(root, key_path_, WorkItem::kWow64Default);
    if (is_string_) {
      items->AddSetRegValueWorkItem(root, key_path_, WorkItem::kWow64Default,
                                    name_, value_, true);
    } else {
      items->AddSetRegValueWorkItem(root, key_path_, WorkItem::kWow64Default,
                                    name_, int_value_, true);
    }
  }
}

bool RegistryEntry::ExistsInRegistry(uint32_t look_for_in) const {
  DCHECK(look_for_in);

  RegistryStatus status = DOES_NOT_EXIST;
  if (look_for_in & LOOK_IN_HKCU)
    status = StatusInRegistryUnderRoot(HKEY_CURRENT_USER);
  if (status == DOES_NOT_EXIST && (look_for_in & LOOK_IN_HKLM))
    status = StatusInRegistryUnderRoot(HKEY_LOCAL_MACHINE);
  return status == SAME_VALUE;
}

bool RegistryEntry::KeyExistsInRegistry(uint32_t look_for_in) const {
  DCHECK(look_for_in);

  RegistryStatus status = DOES_NOT_EXIST;
  if (look_for_in & LOOK_IN_HKCU)
    status = StatusInRegistryUnderRoot(HKEY_CURRENT_USER);
  if (status == DOES_NOT_EXIST && (look_for_in & LOOK_IN_HKLM))
    status = StatusInRegistryUnderRoot(HKEY_LOCAL_MACHINE);
  return status != DOES_NOT_EXIST;
}

RegistryEntry::RegistryStatus RegistryEntry::StatusInRegistryUnderRoot(
    HKEY root) const {
  base::win::RegKey key(root, key_path_.c_str(), KEY_QUERY_VALUE);
  bool found = false;
  bool correct_value = false;
  if (is_string_) {
    std::wstring read_value;
    found = key.ReadValue(name_.c_str(), &read_value) == ERROR_SUCCESS;
    if (found) {
      correct_value =
          read_value.size() == value_.size() &&
          ::CompareString(
              LOCALE_USER_DEFAULT, NORM_IGNORECASE, read_value.data(),
              base::saturated_cast<int>(read_value.size()), value_.data(),
              base::saturated_cast<int>(value_.size())) == CSTR_EQUAL;
    }
  } else {
    DWORD read_value;
    found = key.ReadValueDW(name_.c_str(), &read_value) == ERROR_SUCCESS;
    if (found)
      correct_value = read_value == int_value_;
  }
  return found ? (correct_value ? SAME_VALUE : DIFFERENT_VALUE)
               : DOES_NOT_EXIST;
}
