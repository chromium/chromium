// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/additional_parameters.h"

#include <windows.h>

#include <string_view>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/install_static/install_details.h"
#include "components/version_info/channel.h"

namespace installer {

namespace {

constexpr wchar_t kRegValueAp[] = L"ap";
constexpr std::wstring_view kFullSuffix = L"-full";
constexpr std::wstring_view kExtendedChannel = L"extended";
const wchar_t kTokenSeparator = L'-';

// Returns null if the value was not found or otherwise could not be read.
std::optional<std::wstring> ReadAdditionalParameters() {
  std::optional<std::wstring> result;
  base::win::RegKey key;

  if (key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                 : HKEY_CURRENT_USER,
               install_static::GetClientStateKeyPath().c_str(),
               KEY_WOW64_32KEY | KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    result.emplace();
    if (key.ReadValue(kRegValueAp, &result.value()) != ERROR_SUCCESS)
      result.reset();
  }
  return result;
}

// Writes `value` to the "ap" value in the registry, or deletes the "ap" value
// if `value` is null. Returns false and sets the Windows last-error code on
// failure; otherwise, returns true.
bool WriteAdditionalParameters(const std::optional<std::wstring>& value) {
  base::win::RegKey key;
  LONG result = ERROR_SUCCESS;

  if (!value) {
    // Delete the value if it exists.
    result = key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                        : HKEY_CURRENT_USER,
                      install_static::GetClientStateKeyPath().c_str(),
                      KEY_WOW64_32KEY | KEY_SET_VALUE);
    if (result == ERROR_SUCCESS)
      result = key.DeleteValue(kRegValueAp);
    // Report success if the value was deleted or if either it or the key didn't
    // exist to start with.
    if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ||
        result == ERROR_PATH_NOT_FOUND) {
      return true;
    }
    ::SetLastError(result);
    return false;
  }

  // Write the value to the key.
  result = key.Create(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                        : HKEY_CURRENT_USER,
                      install_static::GetClientStateKeyPath().c_str(),
                      KEY_WOW64_32KEY | KEY_SET_VALUE);
  if (result == ERROR_SUCCESS)
    result = key.WriteValue(kRegValueAp, value->c_str());
  if (result == ERROR_SUCCESS)
    return true;
  ::SetLastError(result);
  return false;
}

bool HasFullSuffix(const std::optional<std::wstring>& value) {
  return value ? base::EndsWith(*value, kFullSuffix) : false;
}

// Expands `channel` to include an optional -arch_FOO suffix, returning true
// if one is found. Returns false without modifying `channel` if none is found.
// `channel` must be a sub-std::string_view of `ap`.
bool SwallowArchSufix(std::wstring_view ap, std::wstring_view& channel) {
  DCHECK_LE(channel.size(), ap.size());
  DCHECK_GE(channel.data(), ap.data());
  DCHECK_LE(channel.data() + channel.size(), ap.data() + ap.size());
  static constexpr std::wstring_view kArchPrefix = L"-arch_";
  auto channel_position = channel.empty() ? 0 : channel.data() - ap.data();
  auto rest_position = channel_position + channel.size();
  if (!base::StartsWith(ap.substr(rest_position), kArchPrefix))
    return false;
  // Retain everything up to the next token separator.
  channel =
      ap.substr(channel_position,
                ap.find(kTokenSeparator, rest_position + kArchPrefix.size()));
  return true;
}

struct ChannelParseState {
  // The canonical name of the parsed channel.
  std::wstring channel_name;

  // The range within the value that matched a channel name pattern.
  std::wstring_view channel_match_range;

  // The range includes an architecture specification (e.g., "x64-beta").
  bool includes_arch;
};

// Parses a channel identifier in `value`, returning the canonical name of the
// channel, the range within `value` that constitutes the identifier, and a
// bool indicating whether or not the identifier also includes an architecture
// specification.
ChannelParseState MakeChannelParseState(
    const std::optional<std::wstring>& value) {
  if (!value)  // No value means stable channel.
    return {std::wstring(), std::wstring_view(), /*includes_arch=*/false};

  std::wstring_view ap(value.value());

  // Expect that "ap" may contain tokens such as "-statsdef_N" and "-full".
  // Historically, all such tokens begin with '-'. Rely on that when matching
  // patterns.

  // Extended stable with an optional -arch_FOO.
  if (base::StartsWith(ap, kExtendedChannel)) {
    std::wstring_view channel_range = ap.substr(0, kExtendedChannel.size());
    bool includes_arch = SwallowArchSufix(ap, channel_range);
    return {std::wstring(kExtendedChannel), channel_range, includes_arch};
  }

  // Beta channel: /^1.1-.*$/ with an optional -arch_FOO. Note that '.' is the
  // regexp wildcard, not a literal period.
  if (ap.size() >= 4 && ap[0] == L'1' && ap[2] == L'1' && ap[3] == L'-') {
    std::wstring_view channel_range = ap.substr(0, ap.find(kTokenSeparator, 4));
    bool includes_arch = SwallowArchSufix(ap, channel_range);
    return {L"beta", channel_range, includes_arch};
  }

  // Dev channel: /^2.0-d.*$/ with an optional -arch_FOO. Note that '.' is the
  // regexp wildcard, not a literal period.
  if (ap.size() >= 5 && ap[0] == L'2' && ap[2] == L'0' && ap[3] == L'-' &&
      ap[4] == L'd') {
    std::wstring_view channel_range = ap.substr(0, ap.find(kTokenSeparator, 5));
    bool includes_arch = SwallowArchSufix(ap, channel_range);
    return {L"dev", channel_range, includes_arch};
  }

  // Older channels.
  static constexpr struct {
    std::wstring_view literal;
    bool is_stable;  // if false, the channel name is embedded in `literal`.
  } kLiteralChannels[] = {
      {L"x64-stable", true}, {L"x86-stable", true}, {L"arm64-stable", true},
      {L"x64-beta", false},  {L"x86-beta", false},  {L"arm64-beta", false},
      {L"x64-dev", false},   {L"x86-dev", false},   {L"arm64-dev", false},
  };
  for (const auto& literal_channel : kLiteralChannels) {
    auto pos = ap.find(literal_channel.literal);
    if (pos == std::wstring_view::npos) {
      continue;
    }
    auto range = ap.substr(pos, literal_channel.literal.size());
    return {literal_channel.is_stable
                ? std::wstring()
                : std::wstring(range.substr(range.find(kTokenSeparator) + 1)),
            range, /*includes_arch=*/true};
  }

  // Explicit stable with an optional -arch_FOO.
  static constexpr std::wstring_view kStableChannel = L"stable";
  if (base::StartsWith(ap, kStableChannel)) {
    std::wstring_view channel_range = ap.substr(0, kStableChannel.size());
    bool includes_arch = SwallowArchSufix(ap, channel_range);
    return {std::wstring(), channel_range, includes_arch};
  }

  // Implicit stable (no channel identifier) with a bare -arch_FOO.
  std::wstring_view channel_range = ap.substr(0, 0);
  if (SwallowArchSufix(ap, channel_range))
    return {std::wstring(), channel_range, /*includes_arch=*/true};

  // Stable channel if nothing else.
  return {std::wstring(), std::wstring_view(), /*includes_arch=*/false};
}

// Returns the channel identifier based on `channel` and
// `is_extended_stable_channel`. If `include_arch`, a processor architecture
// specification is included in the identifier.
std::wstring GetChannelIdentifier(version_info::Channel channel,
                                  bool is_extended_stable_channel,
                                  bool include_arch) {
  static constexpr std::wstring_view kDevChannel = L"2.0-dev";
  static constexpr std::wstring_view kBetaChannel = L"1.1-beta";
#if defined(ARCH_CPU_X86_64)
  static constexpr std::wstring_view kArchSuffix = L"-arch_x64";
#elif defined(ARCH_CPU_X86)
  static constexpr std::wstring_view kArchSuffix = L"-arch_x86";
#elif defined(ARCH_CPU_ARM64)
  static constexpr std::wstring_view kArchSuffix = L"-arch_arm64";
#else
#error unsupported processor architecture.
#endif

  switch (channel) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      NOTREACHED_IN_MIGRATION();
#endif
      return std::wstring();

    case version_info::Channel::DEV:
      if (!include_arch)
        return std::wstring(kDevChannel);
      return base::StrCat({kDevChannel, kArchSuffix});

    case version_info::Channel::BETA:
      if (!include_arch)
        return std::wstring(kBetaChannel);
      return base::StrCat({kBetaChannel, kArchSuffix});

    case version_info::Channel::STABLE:
      if (is_extended_stable_channel) {
        if (!include_arch)
          return std::wstring(kExtendedChannel);
        return base::StrCat({kExtendedChannel, kArchSuffix});
      }
      if (!include_arch)
        return std::wstring();
#if defined(ARCH_CPU_X86_64)
      // For historical reasons, this is the ordinary value for 64-bit stable.
      return L"x64-stable";
#elif defined(ARCH_CPU_X86)
      return L"stable-arch_x86";
#elif defined(ARCH_CPU_ARM64)
      return L"arm64-stable";
#else
#error unsupported processor architecture.
#endif
  }
}

}  // namespace

AdditionalParameters::AdditionalParameters()
    : value_(ReadAdditionalParameters()) {}

AdditionalParameters::~AdditionalParameters() = default;

const wchar_t* AdditionalParameters::value() const {
  return value_ ? value_->c_str() : L"";
}

wchar_t AdditionalParameters::GetStatsDefault() const {
  if (!value_)
    return 0;

  static constexpr std::wstring_view kStatsdef = L"-statsdef_";
  std::wstring_view value(*value_);
  auto pos = value.find(kStatsdef);
  if (pos == std::wstring_view::npos) {
    return 0;
  }
  pos += kStatsdef.size();
  return pos < value.size() ? value[pos] : 0;
}

bool AdditionalParameters::SetFullSuffix(bool set_full_suffix) {
  if (HasFullSuffix(value_) == set_full_suffix)
    return false;  // Nothing to do.
  if (set_full_suffix) {
    if (!value_) {
      value_ = std::wstring(kFullSuffix);
    } else {
      value_->append(kFullSuffix);
    }
  } else {
    DCHECK(value_);
    const auto value_size = value_->size();
    if (value_size == kFullSuffix.size()) {
      value_.reset();
    } else {
      value_->resize(value_size - kFullSuffix.size());
    }
  }
  return true;
}

std::wstring AdditionalParameters::ParseChannel() {
  auto parse_state = MakeChannelParseState(value_);
  return std::move(parse_state.channel_name);
}

void AdditionalParameters::SetChannel(version_info::Channel channel,
                                      bool is_extended_stable_channel) {
  auto parse_state = MakeChannelParseState(value_);

  std::wstring new_value = value_ ? *value_ : std::wstring();

  // Remove the old channel and optional architecture identifier.
  if (!parse_state.channel_match_range.empty()) {
    new_value.erase(parse_state.channel_match_range.data() - value_->data(),
                    parse_state.channel_match_range.size());
  }

  // Insert the new channel and architecture identifier (if needed).
  value_ =
      base::StrCat({GetChannelIdentifier(channel, is_extended_stable_channel,
                                         parse_state.includes_arch),
                    new_value});
}

bool AdditionalParameters::Commit() {
  return WriteAdditionalParameters(value_);
}

}  // namespace installer
