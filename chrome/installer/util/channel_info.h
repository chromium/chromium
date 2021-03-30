// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CHANNEL_INFO_H_
#define CHROME_INSTALLER_UTIL_CHANNEL_INFO_H_

#include <string>

namespace base {
namespace win {
class RegKey;
}
}  // namespace base

namespace installer {

// A helper class for parsing and modifying the Google Update additional
// parameter ("ap") client state value for a product.
class ChannelInfo {
 public:
  // Initialize an instance from the "ap" value in a given registry key.
  // Returns false if the value is present but could not be read from the
  // registry. Returns true if the value was not present or could be read.
  // Also returns true if the key is not valid.
  // An absent "ap" value is treated identically to an empty "ap" value.
  bool Initialize(const base::win::RegKey& key);

  // Writes the info to the "ap" value in a given registry key.
  // Returns false if the value could not be written to the registry.
  bool Write(base::win::RegKey* key) const;

  const std::wstring& value() const { return value_; }
  void set_value(const std::wstring& value) { value_ = value; }
  bool Equals(const ChannelInfo& other) const { return value_ == other.value_; }

  // Removes the -stage: modifier, returning true if the value is modified.
  bool ClearStage();

  // Returns the string identifying the stats default state (i.e., the starting
  // value of the "send usage stats" checkbox during install), or an empty
  // string if the -statsdef_ modifier is not present in the value.
  std::wstring GetStatsDefault() const;

  // Returns true if the -full suffix is present in the value.
  bool HasFullSuffix() const;

  // Adds or removes the -full suffix, returning true if the value is
  // modified.
  bool SetFullSuffix(bool value);

 private:
  std::wstring value_;
};  // class ChannelInfo

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_CHANNEL_INFO_H_
