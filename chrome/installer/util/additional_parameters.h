// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_ADDITIONAL_PARAMETERS_H_
#define CHROME_INSTALLER_UTIL_ADDITIONAL_PARAMETERS_H_

#include <optional>
#include <string>

namespace version_info {
enum class Channel;
}

namespace installer {

// Provides utility functions for accessing and modifying the "additional
// parameters" value stored in the Client State key in the Windows registry.
// This value is included in update checks made by Omaha and is used by the
// update server when selecting a release build.
class AdditionalParameters {
 public:
  // Loads the value from the registry.
  AdditionalParameters();
  AdditionalParameters(const AdditionalParameters&) = delete;
  AdditionalParameters& operator=(const AdditionalParameters&) = delete;
  ~AdditionalParameters();

  // Returns the "ap" value.
  const wchar_t* value() const;

  // Returns the character identifying the stats default state (i.e., the
  // starting value of the "send usage stats" checkbox during install), or zero
  // if the -statsdef_ modifier is not present in the value.
  wchar_t GetStatsDefault() const;

  // Adds or removes the -full suffix, returning true if the value is
  // modified. Commit() must be used to write modified values back to the
  // registry. When such a modification results in an empty value, the "ap"
  // value will be removed from the Windows registry upon Commit().
  bool SetFullSuffix(bool value);

  // Returns the canonical name of the update channel identified by the value.
  // The canonical names of the Google Chrome update channels are "extended",
  // "", "beta", and "dev".
  std::wstring ParseChannel();

  // Updates the channel identifier in the value so that it identifies
  // `channel`. `is_extended_stable_channel` is only used if `channel` is
  // version_info::Channel::STABLE.
  void SetChannel(version_info::Channel channel,
                  bool is_extended_stable_channel);

  // Commits any changes to the Windows registry. Returns true on success.
  // The Windows last-error code is set on failure.
  bool Commit();

 private:
  // null if no value is present in the registry, or if any value in the
  // registry should be removed on commit.
  std::optional<std::wstring> value_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_ADDITIONAL_PARAMETERS_H_
