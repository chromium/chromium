// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/channel_info.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"

using base::win::RegKey;

namespace {

const wchar_t kModStage[] = L"-stage:";
const wchar_t kModStatsDefault[] = L"-statsdef_";
const wchar_t kSfxFull[] = L"-full";

const wchar_t* const kModifiers[] = {
    kModStatsDefault,
    kModStage,
    kSfxFull,
};

enum ModifierIndex { MOD_STATS_DEFAULT, MOD_STAGE, SFX_FULL, NUM_MODIFIERS };

static_assert(NUM_MODIFIERS == base::size(kModifiers),
              "kModifiers disagrees with ModifierIndex; they must match!");

// Returns true if the modifier is found, in which case |position| holds the
// location at which the modifier was found.  The number of characters in the
// modifier is returned in |length|, if non-nullptr.
bool FindModifier(ModifierIndex index,
                  const std::wstring& ap_value,
                  std::wstring::size_type* position,
                  std::wstring::size_type* length) {
  DCHECK_NE(position, nullptr);
  std::wstring::size_type mod_position = std::wstring::npos;
  std::wstring::size_type mod_length =
      std::wstring::traits_type::length(kModifiers[index]);
  char last_char = kModifiers[index][mod_length - 1];
  const bool mod_takes_arg = (last_char == L':' || last_char == L'_');
  std::wstring::size_type pos = 0;
  do {
    mod_position = ap_value.find(kModifiers[index], pos, mod_length);
    if (mod_position == std::wstring::npos)
      return false;  // Modifier not found.
    pos = mod_position + mod_length;
    // Modifiers that take an argument gobble up to the next separator or to the
    // end.
    if (mod_takes_arg) {
      pos = ap_value.find(L'-', pos);
      if (pos == std::wstring::npos)
        pos = ap_value.size();
      break;
    }
    // Regular modifiers must be followed by '-' or the end of the string.
  } while (pos != ap_value.size() && ap_value[pos] != L'-');
  DCHECK_NE(mod_position, std::wstring::npos);
  *position = mod_position;
  if (length != nullptr)
    *length = pos - mod_position;
  return true;
}

bool HasModifier(ModifierIndex index, const std::wstring& ap_value) {
  DCHECK(index >= 0 && index < NUM_MODIFIERS);
  std::wstring::size_type position;
  return FindModifier(index, ap_value, &position, nullptr);
}

std::wstring::size_type FindInsertionPoint(ModifierIndex index,
                                           const std::wstring& ap_value) {
  // Return the location of the next modifier.
  std::wstring::size_type result;

  for (int scan = index + 1; scan < NUM_MODIFIERS; ++scan) {
    if (FindModifier(static_cast<ModifierIndex>(scan), ap_value, &result,
                     nullptr)) {
      return result;
    }
  }

  return ap_value.size();
}

// Returns true if |ap_value| is modified.
bool SetModifier(ModifierIndex index, bool set, std::wstring* ap_value) {
  DCHECK(index >= 0 && index < NUM_MODIFIERS);
  DCHECK(ap_value);
  std::wstring::size_type position;
  std::wstring::size_type length;
  bool have_modifier = FindModifier(index, *ap_value, &position, &length);
  if (set) {
    if (!have_modifier) {
      ap_value->insert(FindInsertionPoint(index, *ap_value), kModifiers[index]);
      return true;
    }
  } else {
    if (have_modifier) {
      ap_value->erase(position, length);
      return true;
    }
  }
  return false;
}

// Returns the value of a modifier - that is for a modifier of the form
// "-foo:bar", returns "bar". Returns an empty string if the modifier
// is not present or does not have a value.
std::wstring GetModifierValue(ModifierIndex modifier_index,
                              const std::wstring& value) {
  std::wstring::size_type position;
  std::wstring::size_type length;

  if (FindModifier(modifier_index, value, &position, &length)) {
    // Return the portion after the prefix.
    std::wstring::size_type pfx_length =
        std::wstring::traits_type::length(kModifiers[modifier_index]);
    DCHECK_LE(pfx_length, length);
    return value.substr(position + pfx_length, length - pfx_length);
  }
  return std::wstring();
}

}  // namespace

namespace installer {

bool ChannelInfo::Initialize(const RegKey& key) {
  LONG result = key.ReadValue(google_update::kRegApField, &value_);
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ||
         result == ERROR_INVALID_HANDLE;
}

bool ChannelInfo::Write(RegKey* key) const {
  DCHECK(key);
  // Google Update deletes the value when it is empty, so we may as well, too.
  LONG result = value_.empty() ? key->DeleteValue(google_update::kRegApField)
                               : key->WriteValue(google_update::kRegApField,
                                                 value_.c_str());
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed writing channel info; result: " << result;
    return false;
  }
  return true;
}

bool ChannelInfo::ClearStage() {
  std::wstring::size_type position;
  std::wstring::size_type length;
  if (FindModifier(MOD_STAGE, value_, &position, &length)) {
    value_.erase(position, length);
    return true;
  }
  return false;
}

std::wstring ChannelInfo::GetStatsDefault() const {
  return GetModifierValue(MOD_STATS_DEFAULT, value_);
}

bool ChannelInfo::HasFullSuffix() const {
  return HasModifier(SFX_FULL, value_);
}

bool ChannelInfo::SetFullSuffix(bool value) {
  return SetModifier(SFX_FULL, value, &value_);
}

}  // namespace installer
