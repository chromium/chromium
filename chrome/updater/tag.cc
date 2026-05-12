// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/tag.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/updater/certificate_tag.h"
#include "chrome/updater/pkg_tag.h"
#include "chrome/updater/tag_internal.h"

#if BUILDFLAG(IS_MAC)
#include <sys/types.h>
#include <sys/xattr.h>
#endif  // BUILDFLAG(IS_MAC)

namespace updater::tagging {
namespace {

// These constants are conceptually cross-platform, but only currently used
// on Mac.
#if BUILDFLAG(IS_MAC)
// Maximum length for the string representation of a tag that can be written
// into a binary. This is the amount of space that must be reserved in a binary
// for dynamic tagging, in a file format where tags can only be patched in place
// rather than inserted, immediately after the magic signature and size bytes.
// Because binary tag format includes an explicit tag size, no null terminator
// is included in this count.
constexpr size_t kMaxTagStringBytes = 8192;

// Maximum length for the binary representation of a tag, including its magic
// signature and length bytes.
constexpr size_t kMaxBinaryTagBytes =
    kMaxTagStringBytes + 2 + kTagMagicUtf8.size();
#endif  // BUILDFLAG(IS_MAC)

// Character that is disallowed from appearing in the tag.
constexpr char kDisallowedCharInTag = '/';

std::optional<NeedsAdmin> ParseNeedsAdminEnum(std::string_view str) {
  if (base::EqualsCaseInsensitiveASCII("false", str)) {
    return NeedsAdmin::kNo;
  }

  if (base::EqualsCaseInsensitiveASCII("true", str)) {
    return NeedsAdmin::kYes;
  }

  if (base::EqualsCaseInsensitiveASCII("prefers", str)) {
    return NeedsAdmin::kPrefers;
  }

  return std::nullopt;
}

// Returns std::nullopt if parsing failed.
std::optional<bool> ParseBool(std::string_view str) {
  if (base::EqualsCaseInsensitiveASCII("false", str)) {
    return false;
  }

  if (base::EqualsCaseInsensitiveASCII("true", str)) {
    return true;
  }

  return std::nullopt;
}

// Functor used by associative containers of strings as a case-insensitive ASCII
// compare. `StringT` could be either UTF-8 or UTF-16.
struct CaseInsensitiveASCIICompare {
 public:
  template <typename StringT>
  bool operator()(const StringT& x, const StringT& y) const {
    return base::CompareCaseInsensitiveASCII(x, y) > 0;
  }
};

namespace global_attributes {

ErrorCode ParseBundleName(std::string_view value, TagArgs& args) {
  value = base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL);
  if (value.empty()) {
    return ErrorCode::kGlobal_BundleNameCannotBeWhitespace;
  }

  args.bundle_name = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseInstallationId(std::string_view value, TagArgs& args) {
  args.installation_id = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseBrandCode(std::string_view value, TagArgs& args) {
  args.brand_code = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseClientId(std::string_view value, TagArgs& args) {
  args.client_id = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseOmahaExperimentLabels(std::string_view value, TagArgs& args) {
  value = base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL);
  if (value.empty()) {
    return ErrorCode::kGlobal_ExperimentLabelsCannotBeWhitespace;
  }

  args.experiment_labels = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseReferralId(std::string_view value, TagArgs& args) {
  args.referral_id = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseBrowserType(std::string_view value, TagArgs& args) {
  int browser_type = 0;
  if (!base::StringToInt(value, &browser_type)) {
    return ErrorCode::kGlobal_BrowserTypeIsInvalid;
  }

  if (browser_type < 0) {
    return ErrorCode::kGlobal_BrowserTypeIsInvalid;
  }

  args.browser_type =
      browser_type < std::to_underlying(TagArgs::BrowserType::kMax)
          ? TagArgs::BrowserType(browser_type)
          : TagArgs::BrowserType::kUnknown;

  return ErrorCode::kSuccess;
}

ErrorCode ParseLanguage(std::string_view value, TagArgs& args) {
  // Even if we don't support the language, we want to pass it to the
  // installer. Omaha will pick its language later. See http://b/1336966.
  args.language = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseFlighting(std::string_view value, TagArgs& args) {
  const std::optional<bool> flighting = ParseBool(value);
  if (!flighting.has_value()) {
    return ErrorCode::kGlobal_FlightingValueIsNotABoolean;
  }

  args.flighting = flighting.value();
  return ErrorCode::kSuccess;
}

ErrorCode ParseUsageStats(std::string_view value, TagArgs& args) {
  int tristate = 0;
  if (!base::StringToInt(value, &tristate)) {
    return ErrorCode::kGlobal_UsageStatsValueIsInvalid;
  }

  if (tristate == 0) {
    args.usage_stats_enable = false;
  } else if (tristate == 1) {
    args.usage_stats_enable = true;
  } else if (tristate == 2) {
    args.usage_stats_enable = std::nullopt;
  } else {
    return ErrorCode::kGlobal_UsageStatsValueIsInvalid;
  }
  return ErrorCode::kSuccess;
}

// Parses an app ID and adds it to the list of apps in |args|, if valid.
ErrorCode ParseAppId(std::string_view value, TagArgs& args) {
  if (!base::IsStringASCII(value)) {
    return ErrorCode::kApp_AppIdIsNotValid;
  }

  args.apps.push_back(AppArgs(value));
  return ErrorCode::kSuccess;
}

ErrorCode ParseRuntimeMode(std::string_view value, TagArgs& args) {
  for (const std::string_view expected_value : {"true", "persist", "false"}) {
    if (base::EqualsCaseInsensitiveASCII(expected_value, value)) {
      args.runtime_mode = RuntimeModeArgs();
      return ErrorCode::kSuccess;
    }
  }

  return ErrorCode::kGlobal_RuntimeModeValueIsInvalid;
}

ErrorCode ParseEnrollmentToken(std::string_view value, TagArgs& args) {
  if (!base::Uuid::ParseCaseInsensitive(value).is_valid()) {
    return ErrorCode::kGlobal_EnrollmentTokenValueIsInvalid;
  }
  args.enrollment_token = value;
  return ErrorCode::kSuccess;
}

// |value| must not be empty.
using ParseGlobalAttributeFunPtr = ErrorCode (*)(std::string_view value,
                                                 TagArgs& args);

using GlobalParseTable = std::map<std::string_view,
                                  ParseGlobalAttributeFunPtr,
                                  CaseInsensitiveASCIICompare>;

const GlobalParseTable& GetTable() {
  static const base::NoDestructor<GlobalParseTable> instance{
      {{kTagArgBundleName, &ParseBundleName},
       {kTagArgInstallationId, &ParseInstallationId},
       {kTagArgBrandCode, &ParseBrandCode},
       {kTagArgClientId, &ParseClientId},
       {kTagArgOmahaExperimentLabels, &ParseOmahaExperimentLabels},
       {kTagArgReferralId, &ParseReferralId},
       {kTagArgBrowserType, &ParseBrowserType},
       {kTagArgLanguage, &ParseLanguage},
       {kTagArgFlighting, &ParseFlighting},
       {kTagArgUsageStats, &ParseUsageStats},
       {kTagArgAppId, &ParseAppId},
       {kTagArgRuntimeMode, &ParseRuntimeMode},
       {kTagArgEnrollmentToken, &ParseEnrollmentToken}}};
  return *instance;
}

}  // namespace global_attributes

namespace app_attributes {

ErrorCode ParseAdditionalParameters(std::string_view value, AppArgs& args) {
  args.ap = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseExperimentLabels(std::string_view value, AppArgs& args) {
  value = base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL);
  if (value.empty()) {
    return ErrorCode::kApp_ExperimentLabelsCannotBeWhitespace;
  }

  args.experiment_labels = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseAppName(std::string_view value, AppArgs& args) {
  value = base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL);
  if (value.empty()) {
    return ErrorCode::kApp_AppNameCannotBeWhitespace;
  }

  args.app_name = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseNeedsAdmin(std::string_view value, AppArgs& args) {
  const auto needs_admin = ParseNeedsAdminEnum(value);
  if (!needs_admin.has_value()) {
    return ErrorCode::kApp_NeedsAdminValueIsInvalid;
  }

  args.needs_admin = needs_admin.value();
  return ErrorCode::kSuccess;
}

ErrorCode ParseInstallDataIndex(std::string_view value, AppArgs& args) {
  args.install_data_index = value;
  return ErrorCode::kSuccess;
}

ErrorCode ParseUntrustedData(std::string_view value, AppArgs& args) {
  args.untrusted_data = value;
  return ErrorCode::kSuccess;
}

// |value| must not be empty.
using ParseAppAttributeFunPtr = ErrorCode (*)(std::string_view value,
                                              AppArgs& args);

using AppParseTable = std::
    map<std::string_view, ParseAppAttributeFunPtr, CaseInsensitiveASCIICompare>;

const AppParseTable& GetTable() {
  static const base::NoDestructor<AppParseTable> instance{{
      {kAppArgAdditionalParameters, &ParseAdditionalParameters},
      {kAppArgExperimentLabels, &ParseExperimentLabels},
      {kAppArgAppName, &ParseAppName},
      {kTagArgNeedsAdmin, &ParseNeedsAdmin},
      {kAppArgInstallDataIndex, &ParseInstallDataIndex},
      {kAppArgUntrustedData, &ParseUntrustedData},
  }};
  return *instance;
}

}  // namespace app_attributes

namespace runtime_mode_attributes {

ErrorCode ParseNeedsAdmin(std::string_view value, RuntimeModeArgs& args) {
  const auto needs_admin = ParseNeedsAdminEnum(value);
  if (!needs_admin.has_value()) {
    return ErrorCode::kRuntimeMode_NeedsAdminValueIsInvalid;
  }

  args.needs_admin = needs_admin.value();
  return ErrorCode::kSuccess;
}

// |value| must not be empty.
using ParseRuntimeModeAttributeFunPtr = ErrorCode (*)(std::string_view value,
                                                      RuntimeModeArgs& args);

using RuntimeModeParseTable = std::map<std::string_view,
                                       ParseRuntimeModeAttributeFunPtr,
                                       CaseInsensitiveASCIICompare>;

const RuntimeModeParseTable& GetTable() {
  static const base::NoDestructor<RuntimeModeParseTable> instance{{
      {kTagArgNeedsAdmin, &ParseNeedsAdmin},
  }};
  return *instance;
}

}  // namespace runtime_mode_attributes

namespace installer_data_attributes {

// Search for the given appid specified by |value| in |args.apps| and write its
// index to |current_app_index|.
ErrorCode FindAppIdInTagArgs(std::string_view value,
                             TagArgs& args,
                             std::optional<size_t>& current_app_index) {
  if (!base::IsStringASCII(value)) {
    return ErrorCode::kApp_AppIdIsNotValid;
  }

  // Find the app in the existing list.
  for (size_t i = 0; i < args.apps.size(); i++) {
    if (base::EqualsCaseInsensitiveASCII(args.apps[i].app_id, value)) {
      current_app_index = i;
    }
  }

  if (!current_app_index.has_value()) {
    return ErrorCode::kAppInstallerData_AppIdNotFound;
  }

  return ErrorCode::kSuccess;
}

ErrorCode ParseInstallerData(std::string_view value,
                             TagArgs& args,
                             std::optional<size_t>& current_app_index) {
  if (!current_app_index.has_value()) {
    return ErrorCode::
        kAppInstallerData_InstallerDataCannotBeSpecifiedBeforeAppId;
  }

  args.apps[current_app_index.value()].encoded_installer_data = value;

  return ErrorCode::kSuccess;
}

// |value| must not be empty.
// |current_app_index| is an in/out parameter. It stores the index of the
// current app and nullopt if no app has been set yet. Writing to it will set
// the index for future calls to these functions.
using ParseInstallerDataAttributeFunPtr =
    ErrorCode (*)(std::string_view value,
                  TagArgs& args,
                  std::optional<size_t>& current_app_index);

using InstallerDataParseTable = std::map<std::string_view,
                                         ParseInstallerDataAttributeFunPtr,
                                         CaseInsensitiveASCIICompare>;

const InstallerDataParseTable& GetTable() {
  static const base::NoDestructor<InstallerDataParseTable> instance{{
      {kTagArgAppId, &FindAppIdInTagArgs},
      {kAppArgInstallerData, &ParseInstallerData},
  }};
  return *instance;
}

}  // namespace installer_data_attributes

namespace query_string {

// An attribute in a metainstaller tag or app installer data args string.
// - The first value is the "name" of the attribute.
// - The second value is the "value" of the attribute.
using Attribute = std::pair<std::string, std::string>;

// Splits |query_string| into |Attribute|s. Attribute values will be unescaped
// if |unescape_value| is true.
//
// Ownership follows the same rules as |base::SplitStringPiece|.
std::vector<Attribute> Split(std::string_view query_string,
                             bool unescape_value = true) {
  std::vector<Attribute> attributes;
  for (const auto& attribute_string :
       base::SplitStringPiece(query_string, "&", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    size_t separate_pos = attribute_string.find_first_of("=");
    if (separate_pos == std::string_view::npos) {
      // Add a name-only attribute.
      std::string_view name = base::TrimWhitespaceASCII(
          attribute_string, base::TrimPositions::TRIM_ALL);
      attributes.emplace_back(std::string{name}, "");
    } else {
      std::string_view name =
          base::TrimWhitespaceASCII(attribute_string.substr(0, separate_pos),
                                    base::TrimPositions::TRIM_ALL);
      std::string_view value =
          base::TrimWhitespaceASCII(attribute_string.substr(separate_pos + 1),
                                    base::TrimPositions::TRIM_ALL);
      attributes.emplace_back(
          name,
          unescape_value
              ? base::UnescapeURLComponent(
                    value, base::UnescapeRule::SPACES |
                               base::UnescapeRule::
                                   URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
                               base::UnescapeRule::PATH_SEPARATORS)
              : std::string{value});
    }
  }
  return attributes;
}

}  // namespace query_string

// Parses global and app-specific attributes from |tag|.
ErrorCode ParseTag(std::string_view tag, TagArgs& args) {
  const auto& global_func_lookup_table = global_attributes::GetTable();
  const auto& app_func_lookup_table = app_attributes::GetTable();
  const auto& runtime_mode_func_lookup_table =
      runtime_mode_attributes::GetTable();

  const std::vector<std::pair<std::string, std::string>> attributes =
      query_string::Split(tag);
  for (const auto& [name, value] : attributes) {
    // Attribute names are only ASCII, so no i18n case folding needed.
    if (global_func_lookup_table.contains(name)) {
      if (value.empty()) {
        return ErrorCode::kAttributeMustHaveValue;
      }

      const ErrorCode result = global_func_lookup_table.at(name)(value, args);
      if (result != ErrorCode::kSuccess) {
        return result;
      }
    } else if ((runtime_mode_func_lookup_table.contains(name)) &&
               args.runtime_mode) {
      if (value.empty()) {
        return ErrorCode::kAttributeMustHaveValue;
      }

      const ErrorCode result =
          runtime_mode_func_lookup_table.at(name)(value, *args.runtime_mode);
      if (result != ErrorCode::kSuccess) {
        return result;
      }
    } else if (app_func_lookup_table.contains(name)) {
      if (args.apps.empty()) {
        return ErrorCode::kApp_AppIdNotSpecified;
      }

      if (value.empty()) {
        return ErrorCode::kAttributeMustHaveValue;
      }

      AppArgs& current_app = args.apps.back();
      const ErrorCode result =
          app_func_lookup_table.at(name)(value, current_app);
      if (result != ErrorCode::kSuccess) {
        return result;
      }
    } else {
      return ErrorCode::kUnrecognizedName;
    }
  }

  // The bundle name inherits the first app's name, if not set.
  if (args.bundle_name.empty() && !args.apps.empty()) {
    args.bundle_name = args.apps[0].app_name;
  }
  args.tag_string = tag;
  args.attributes = attributes;

  return ErrorCode::kSuccess;
}

// Parses app-specific installer data from |app_installer_data_args|.
ErrorCode ParseAppInstallerDataArgs(std::string_view app_installer_data_args,
                                    TagArgs& args) {
  // The currently tracked app index to apply installer data to.
  std::optional<size_t> current_app_index;

  // Installer data is assumed to be URL-encoded, so we don't unescape it.
  bool unescape_value = false;

  for (const auto& [name, value] :
       query_string::Split(app_installer_data_args, unescape_value)) {
    if (value.empty()) {
      return ErrorCode::kAttributeMustHaveValue;
    }

    const auto& func_lookup_table = installer_data_attributes::GetTable();
    if (!func_lookup_table.contains(name)) {
      return ErrorCode::kUnrecognizedName;
    }

    const ErrorCode result =
        func_lookup_table.at(name)(value, args, current_app_index);
    if (result != ErrorCode::kSuccess) {
      return result;
    }
  }

  return ErrorCode::kSuccess;
}

// Checks that |args| does not contain |kDisallowedCharInTag|.
bool IsValidArgs(std::string_view args) {
  return !args.contains(kDisallowedCharInTag);
}

// Returns a `uint16_t` value as big-endian bytes.
std::array<uint8_t, 2> U16IntToBigEndian(uint16_t value) {
  return {static_cast<uint8_t>((value & 0xFF00) >> 8),
          static_cast<uint8_t>(value & 0x00FF)};
}

// Loads up to the last 80K bytes from `filename`.
std::vector<uint8_t> ReadFileTail(const base::FilePath& filename) {
  static constexpr size_t kMaxBytesToRead = 81920;  // 80K

  base::File file(filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return {};
  }

  const int64_t file_length = file.GetLength();
  if (file_length < 0) {
    LOG(ERROR) << "Valid file, invalid length at path: " << filename;
    return {};
  }
  const size_t unsigned_file_length = base::checked_cast<size_t>(file_length);
  const size_t bytes_to_read = std::min(unsigned_file_length, kMaxBytesToRead);
  const int64_t offset =
      (unsigned_file_length > bytes_to_read) ? file_length - bytes_to_read : 0;

  std::vector<uint8_t> buffer(bytes_to_read);
  return file.ReadAndCheck(offset, base::span(buffer)) ? buffer
                                                       : std::vector<uint8_t>();
}

std::string ParseTagBuffer(const std::vector<uint8_t>& tag_buffer) {
  if (tag_buffer.empty()) {
    return {};
  }

  const std::string tag_string = ReadTag(tag_buffer);
  LOG_IF(ERROR, tag_string.empty()) << __func__ << ": Tag not found in file.";
  return tag_string;
}

std::vector<uint8_t> ReadEntireFile(const base::FilePath& file) {
  std::optional<int64_t> file_size = base::GetFileSize(file);
  if (!file_size.has_value()) {
    PLOG(ERROR) << __func__ << ": Could not get file size: " << file;
    return {};
  }

  std::vector<uint8_t> contents(file_size.value());
  if (base::ReadFile(file, reinterpret_cast<char*>(&contents.front()),
                     contents.size()) == -1) {
    PLOG(ERROR) << __func__ << ": Could not read file: " << file;
    return {};
  }
  return contents;
}

}  // namespace

namespace internal {
std::vector<uint8_t>::const_iterator AdvanceIt(
    std::vector<uint8_t>::const_iterator it,
    size_t distance,
    std::vector<uint8_t>::const_iterator end) {
  if (it >= end) {
    return end;
  }

  ptrdiff_t dist_to_end = 0;
  if (!base::CheckedNumeric<ptrdiff_t>(end - it).AssignIfValid(&dist_to_end)) {
    return end;
  }

  return it + std::min(distance, static_cast<size_t>(dist_to_end));
}

bool CheckRange(std::vector<uint8_t>::const_iterator it,
                size_t size,
                std::vector<uint8_t>::const_iterator end) {
  if (it >= end || size == 0) {
    return false;
  }

  ptrdiff_t dist_to_end = 0;
  if (!base::CheckedNumeric<ptrdiff_t>(end - it).AssignIfValid(&dist_to_end)) {
    return false;
  }

  return size <= static_cast<size_t>(dist_to_end);
}
}  // namespace internal

AppArgs::AppArgs(std::string_view app_id) : app_id(base::ToLowerASCII(app_id)) {
  CHECK(!app_id.empty());
}

AppArgs::~AppArgs() = default;
AppArgs::AppArgs(const AppArgs&) = default;
AppArgs& AppArgs::operator=(const AppArgs&) = default;
AppArgs::AppArgs(AppArgs&&) = default;
AppArgs& AppArgs::operator=(AppArgs&&) = default;

TagArgs::TagArgs() = default;
TagArgs::~TagArgs() = default;
TagArgs::TagArgs(const TagArgs&) = default;
TagArgs& TagArgs::operator=(const TagArgs&) = default;
TagArgs::TagArgs(TagArgs&&) = default;
TagArgs& TagArgs::operator=(TagArgs&&) = default;

ErrorCode Parse(std::string_view tag,
                std::optional<std::string_view> app_installer_data_args,
                TagArgs& args) {
  if (!IsValidArgs(tag)) {
    return ErrorCode::kTagIsInvalid;
  }

  const ErrorCode result = ParseTag(tag, args);
  if (result != ErrorCode::kSuccess) {
    return result;
  }

  if (!app_installer_data_args.has_value()) {
    return ErrorCode::kSuccess;
  }

  if (!IsValidArgs(app_installer_data_args.value())) {
    return ErrorCode::kTagIsInvalid;
  }

  return ParseAppInstallerDataArgs(app_installer_data_args.value(), args);
}

std::ostream& operator<<(std::ostream& os, const ErrorCode& error_code) {
  switch (error_code) {
    case ErrorCode::kSuccess:
      return os << "ErrorCode::kSuccess";
    case ErrorCode::kUnrecognizedName:
      return os << "ErrorCode::kUnrecognizedName";
    case ErrorCode::kTagIsInvalid:
      return os << "ErrorCode::kTagIsInvalid";
    case ErrorCode::kAttributeMustHaveValue:
      return os << "ErrorCode::kAttributeMustHaveValue";
    case ErrorCode::kApp_AppIdNotSpecified:
      return os << "ErrorCode::kApp_AppIdNotSpecified";
    case ErrorCode::kApp_ExperimentLabelsCannotBeWhitespace:
      return os << "ErrorCode::kApp_ExperimentLabelsCannotBeWhitespace";
    case ErrorCode::kApp_AppIdIsNotValid:
      return os << "ErrorCode::kApp_AppIdIsNotValid";
    case ErrorCode::kApp_AppNameCannotBeWhitespace:
      return os << "ErrorCode::kApp_AppNameCannotBeWhitespace";
    case ErrorCode::kApp_NeedsAdminValueIsInvalid:
      return os << "ErrorCode::kApp_NeedsAdminValueIsInvalid";
    case ErrorCode::kAppInstallerData_AppIdNotFound:
      return os << "ErrorCode::kAppInstallerData_AppIdNotFound";
    case ErrorCode::kAppInstallerData_InstallerDataCannotBeSpecifiedBeforeAppId:
      return os << "ErrorCode::kAppInstallerData_"
                   "InstallerDataCannotBeSpecifiedBeforeAppId";
    case ErrorCode::kGlobal_BundleNameCannotBeWhitespace:
      return os << "ErrorCode::kGlobal_BundleNameCannotBeWhitespace";
    case ErrorCode::kGlobal_ExperimentLabelsCannotBeWhitespace:
      return os << "ErrorCode::kGlobal_ExperimentLabelsCannotBeWhitespace";
    case ErrorCode::kGlobal_BrowserTypeIsInvalid:
      return os << "ErrorCode::kGlobal_BrowserTypeIsInvalid";
    case ErrorCode::kGlobal_FlightingValueIsNotABoolean:
      return os << "ErrorCode::kGlobal_FlightingValueIsNotABoolean";
    case ErrorCode::kGlobal_UsageStatsValueIsInvalid:
      return os << "ErrorCode::kGlobal_UsageStatsValueIsInvalid";
    case ErrorCode::kGlobal_RuntimeModeValueIsInvalid:
      return os << "ErrorCode::kGlobal_RuntimeModeValueIsInvalid";
    case ErrorCode::kGlobal_EnrollmentTokenValueIsInvalid:
      return os << "ErrorCode::kGlobal_EnrollmentTokenValueIsInvalid";
    case ErrorCode::kRuntimeMode_NeedsAdminValueIsInvalid:
      return os << "ErrorCode::kRuntimeMode_NeedsAdminValueIsInvalid";
    case ErrorCode::kTagNotFound:
      return os << "ErrorCode::kTagNotFound";
  }
}

std::ostream& operator<<(std::ostream& os, const NeedsAdmin& needs_admin) {
  switch (needs_admin) {
    case NeedsAdmin::kNo:
      return os << "NeedsAdmin::kNo";
    case NeedsAdmin::kYes:
      return os << "NeedsAdmin::kYes";
    case NeedsAdmin::kPrefers:
      return os << "NeedsAdmin::kPrefers";
  }
}

std::ostream& operator<<(std::ostream& os,
                         const TagArgs::BrowserType& browser_type) {
  switch (browser_type) {
    case TagArgs::BrowserType::kUnknown:
      return os << "TagArgs::BrowserType::kUnknown";
    case TagArgs::BrowserType::kDefault:
      return os << "TagArgs::BrowserType::kDefault";
    case TagArgs::BrowserType::kInternetExplorer:
      return os << "TagArgs::BrowserType::kInternetExplorer";
    case TagArgs::BrowserType::kFirefox:
      return os << "TagArgs::BrowserType::kFirefox";
    case TagArgs::BrowserType::kChrome:
      return os << "TagArgs::BrowserType::kChrome";
    default:
      return os << "TagArgs::BrowserType(" << browser_type << ")";
  }
}

std::vector<uint8_t> GetTagFromTagString(const std::string& tag_string) {
  std::vector<uint8_t> tag(std::from_range, kTagMagicUtf8);
  const std::array<uint8_t, 2> tag_length =
      U16IntToBigEndian(tag_string.length());
  tag.insert(tag.end(), tag_length.begin(), tag_length.end());
  tag.insert(tag.end(), tag_string.begin(), tag_string.end());
  return tag;
}

ReadTagResult ReadTagAndOffset(base::span<const uint8_t> buffer) {
  auto magic_str_subrange = std::ranges::find_end(buffer, kTagMagicUtf8);
  auto magic_str_begin = magic_str_subrange.begin();
  if (magic_str_begin == buffer.end()) {
    return NoTagFound{};
  }

  size_t magic_offset =
      base::checked_cast<size_t>(magic_str_begin - buffer.begin());
  size_t taglen_offset = magic_offset + kTagMagicUtf8.size();

  base::SpanReader reader(buffer.subspan(taglen_offset));
  uint16_t tag_len = 0;
  if (!reader.ReadU16BigEndian(tag_len)) {
    return InvalidTag{.offset = magic_offset};
  }

  std::optional<base::span<const uint8_t>> tag_span = reader.Read(tag_len);
  if (!tag_span) {
    return InvalidTag{.offset = magic_offset};
  }

  return ValidTag{.offset = magic_offset,
                  .data = std::string(base::as_string_view(*tag_span))};
}

std::string ReadTag(base::span<const uint8_t> buffer) {
  ReadTagResult result = ReadTagAndOffset(buffer);
  if (const auto* valid_tag = std::get_if<ValidTag>(&result)) {
    return valid_tag->data;
  }
  return std::string();
}

std::unique_ptr<tagging::BinaryInterface> CreateBinary(
    const base::FilePath& file,
    base::span<const uint8_t> contents) {
  if (file.MatchesExtension(FILE_PATH_LITERAL(".exe"))) {
    return CreatePEBinary(contents);
  } else if (file.MatchesExtension(FILE_PATH_LITERAL(".msi"))) {
    return CreateMSIBinary(contents);
  } else if (file.MatchesExtension(FILE_PATH_LITERAL(".pkg"))) {
    return CreatePkgBinary(contents);
  } else {
    std::unique_ptr<BinaryInterface> binary = CreatePEBinary(contents);
    if (!binary) {
      binary = CreateMSIBinary(contents);
    }
    return binary;
  }
}

std::string BinaryReadTagString(const base::FilePath& file) {
  if (file.MatchesExtension(FILE_PATH_LITERAL(".pkg"))) {
    return ReadTagFromPkg(ReadFileTail(file));
  }

  // For MSI files, simply search the tail of the file for the tag.
  if (!file.MatchesExtension(FILE_PATH_LITERAL(".exe"))) {
    return ParseTagBuffer(ReadFileTail(file));
  }

  base::MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(file)) {
    LOG(ERROR) << __func__ << ": Unknown or empty file: " << file;
    return {};
  }
  std::unique_ptr<tagging::BinaryInterface> bin =
      CreateBinary(file, mapped_file.bytes());
  if (!bin) {
    LOG(ERROR) << __func__ << ": Could not parse binary: " << file;
    return {};
  }

  std::optional<std::vector<uint8_t>> tag = bin->tag();
  if (!tag) {
    LOG(ERROR) << __func__ << ": No superfluous certificate in file: " << file;
    return {};
  }

  const std::vector<uint8_t> tag_data = {tag->begin(), tag->end()};
  const std::string tag_string = ReadTag(tag_data);
  if (tag_string.empty()) {
    LOG(ERROR) << __func__ << ": file is untagged: " << file;
  }
  return tag_string;
}

std::optional<tagging::TagArgs> BinaryReadTag(const base::FilePath& file) {
  const std::string tag_string = BinaryReadTagString(file);
  if (tag_string.empty()) {
    return {};
  }
  tagging::TagArgs tag_args;
  const tagging::ErrorCode error = tagging::Parse(tag_string, {}, tag_args);
  if (error != tagging::ErrorCode::kSuccess) {
    LOG(ERROR) << __func__ << ": Invalid tag string: " << tag_string << ": "
               << error;
    return {};
  }
  return tag_args;
}

bool BinaryWriteTag(const base::FilePath& in_file,
                    const std::string& tag_string,
                    int padded_length,
                    base::FilePath out_file) {
  const std::vector<uint8_t> contents = ReadEntireFile(in_file);
  std::unique_ptr<tagging::BinaryInterface> bin =
      CreateBinary(in_file, contents);
  if (!bin) {
    LOG(ERROR) << __func__ << ": Could not parse binary: " << in_file;
    return false;
  }

  // Validate the tag string, if any.
  if (!tag_string.empty()) {
    tagging::TagArgs tag_args;
    const tagging::ErrorCode error = tagging::Parse(tag_string, {}, tag_args);
    if (error != tagging::ErrorCode::kSuccess) {
      LOG(ERROR) << __func__ << ": Invalid tag string: " << tag_string << ": "
                 << error;
      return false;
    }
  }

  std::vector<uint8_t> tag_contents = tagging::GetTagFromTagString(tag_string);

  if (padded_length > 0) {
    size_t new_size = 0;
    if (base::CheckAdd(tag_contents.size(), padded_length)
            .AssignIfValid(&new_size)) {
      tag_contents.resize(new_size);
    } else {
      LOG(ERROR) << __func__ << "Failed to pad the tag contents.";
      return false;
    }
  }

  auto new_contents = bin->SetTag(tag_contents);
  if (!new_contents) {
    LOG(ERROR) << __func__ << "Error while setting binary tag.";
    return false;
  }
  if (out_file.empty()) {
    out_file = in_file;
  }
  if (!base::WriteFile(out_file, *new_contents)) {
    PLOG(ERROR) << __func__ << "Error while writing updated file: " << out_file;
    return false;
  }
  return true;
}

#if BUILDFLAG(IS_MAC)

base::expected<TagArgs, ErrorCode> ReadTagFromApplicationInstanceXattr(
    const base::FilePath& path) {
  if (path.empty()) {
    VLOG(0) << "no path in ReadTagFromApplicationInstanceXattr";
    return base::unexpected(ErrorCode::kTagNotFound);
  }

  std::vector<uint8_t> raw_tag(kMaxBinaryTagBytes, 0);
  ssize_t got_bytes =
      getxattr(path.value().c_str(), "com.apple.application-instance",
               raw_tag.data(), kMaxBinaryTagBytes, 0, 0);
  // If a C API says it wrote past the end of a buffer, believe it.
  CHECK(got_bytes <= static_cast<ssize_t>(kMaxBinaryTagBytes))
      << "getxattr wrote " << got_bytes << " bytes into a "
      << kMaxBinaryTagBytes << " byte buffer!";
  if (got_bytes < 0) {
    VPLOG(1) << "getxattr could not read com.apple.application-instance on "
             << path;
    return base::unexpected(ErrorCode::kTagNotFound);
  }
  std::string tag_string =
      ReadTag(base::span(raw_tag).first(base::checked_cast<size_t>(got_bytes)));
  if (tag_string.empty()) {
    return base::unexpected(ErrorCode::kTagNotFound);
  }
  TagArgs value;
  ErrorCode code = Parse(tag_string, {}, value);
  if (code != ErrorCode::kSuccess) {
    return base::unexpected(code);
  }
  return value;
}

bool WriteTagStringToApplicationInstanceXattr(const base::FilePath& path,
                                              const std::string& tag_string) {
  if (path.empty()) {
    VLOG(0) << "no path provided when writing xattr tag";
    return false;
  }
  if (tag_string.size() > kMaxTagStringBytes) {
    VLOG(1) << "xattr tag too big, will be truncated when read";
    // warning only; continue
  }
  if (tag_string.empty()) {
    VLOG(1) << "writing empty xattr tag";
    // warning only; continue
  }
  std::vector<uint8_t> tag_bytes = GetTagFromTagString(tag_string);
  if (tag_bytes.empty()) {
    VLOG(0) << "could not create xattr tag";
    return false;
  }
  int result = setxattr(path.value().c_str(), "com.apple.application-instance",
                        tag_bytes.data(), tag_bytes.size(), 0, 0);
  if (result) {
    VPLOG(0) << "setxattr failed on " << path;
    return false;
  }
  return true;
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace updater::tagging
