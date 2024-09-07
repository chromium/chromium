// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/tag.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "chrome/updater/certificate_tag.h"

namespace updater::tagging {
namespace {

// The name of the bundle being installed. If not specified, the first app's
// appname is used.
constexpr std::string_view kTagArgBundleName = "bundlename";

// The language of the product the user is installing.
constexpr std::string_view kTagArgLanguage = "lang";

// Flag denoting that the user is flighting a new test feature.
constexpr std::string_view kTagArgFlighting = "flighting";

// Flag denoting that the user has agreed to provide usage stats, crashreports
// etc.
constexpr std::string_view kTagArgUsageStats = "usagestats";

// A unique value for this installation session. It can be used to follow the
// progress from the website to installation completion.
constexpr std::string_view kTagArgInstallationId = "iid";

// The brand code used for branding. This value sets the initial brand for the
// updater and the client app. If a brand value already exists on the system,
// the new brand value is ignored.
constexpr std::string_view kTagArgBrandCode = "brand";

// The Client ID used for branding.
// If a client value already exists on the system, it should be ignored.
// This value is used to set the initial client for the updater and the client
// app.
constexpr std::string_view kTagArgClientId = "client";

// A set of experiment labels used to track installs that are included in
// experiments. Use "experiments" for per-app arguments; use "omahaexperiments"
// for updater-specific labels.
constexpr std::string_view kAppArgExperimentLabels = "experiments";
constexpr std::string_view kTagArgOmahaExperimentLabels = "omahaexperiments";

// A referral ID used for tracking referrals.
constexpr std::string_view kTagArgReferralId = "referral";

// Tells the updater what ap value to set in the registry.
constexpr std::string_view kAppArgAdditionalParameters = "ap";

// Indicates which browser to restart on successful install.
constexpr std::string_view kTagArgBrowserType = "browser";

// Runtime Mode: "runtime" argument in the tag tells the updater to install
// itself and stay on the system without any associated application for at least
// `kMaxServerStartsBeforeFirstReg` wakes. This feature is used to expose the
// COM API to a process that will install applications via that API.
// Example:
//   "runtime=true&needsadmin=true"
constexpr std::string_view kTagArgRuntimeMode = "runtime";

// Enrollment token: "etoken" argument in the tag tells the per-machine updater
// to register the machine to the device management server. The value must be a
// GUID.
// Example:
//   "etoken=5d086552-4514-4dfb-8a3e-337024ec35ac"
constexpr std::string_view kTagArgErollmentToken = "etoken";

// The list of arguments that are needed for a meta-installer, to
// indicate which application is being installed. These are stamped
// inside the meta-installer binary.
constexpr std::string_view kTagArgAppId = "appguid";
constexpr std::string_view kAppArgAppName = "appname";
constexpr std::string_view kTagArgNeedsAdmin = "needsadmin";
constexpr std::string_view kAppArgInstallDataIndex = "installdataindex";
constexpr std::string_view kAppArgUntrustedData = "untrusteddata";

// This switch allows extra data to be communicated to the application
// installer. The extra data needs to be URL-encoded. The data will be decoded
// and written to the file, that is then passed in the command line to the
// application installer in the form "/installerdata=blah.dat". One per
// application.
constexpr std::string_view kAppArgInstallerData = "installerdata";

// Character that is disallowed from appearing in the tag.
constexpr char kDisallowedCharInTag = '/';

// Magic string used to identify the tag in the binary.
constexpr uint8_t kTagMagicUtf8[] = {'G', 'a', 'c', 't', '2', '.',
                                     '0', 'O', 'm', 'a', 'h', 'a'};

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
      browser_type < base::to_underlying(TagArgs::BrowserType::kMax)
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
       {kTagArgErollmentToken, &ParseEnrollmentToken}}};
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
  return !base::Contains(args, kDisallowedCharInTag);
}

// Returns a `uint16_t` value as big-endian bytes.
std::array<uint8_t, 2> U16IntToBigEndian(uint16_t value) {
  return {static_cast<uint8_t>((value & 0xFF00) >> 8),
          static_cast<uint8_t>(value & 0x00FF)};
}

// Converts a big-endian 2-byte value to little-endian and returns it
// as a uint16_t.
uint16_t BigEndianReadU16(std::vector<uint8_t>::const_iterator it) {
  static_assert(ARCH_CPU_LITTLE_ENDIAN, "Machine should be little-endian.");
  return (uint16_t{*it} << 8) + (uint16_t{*(it + 1)});
}

// Loads up to the last 80K bytes from `filename`.
std::vector<uint8_t> ReadFileTail(const base::FilePath& filename) {
  constexpr int64_t kMaxBytesToRead = 81920;  // 80K

  base::File file(filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return {};
  }

  const int64_t file_length = file.GetLength();
  const int64_t bytes_to_read = std::min(file_length, kMaxBytesToRead);
  const int64_t offset =
      (file_length > bytes_to_read) ? file_length - bytes_to_read : 0;

  std::vector<uint8_t> buffer(bytes_to_read);
  const int num_bytes_read =
      file.Read(offset, reinterpret_cast<char*>(&buffer[0]), bytes_to_read);
  if (num_bytes_read != bytes_to_read) {
    return {};
  }

  return buffer;
}

std::string ParseTagBuffer(const std::vector<uint8_t>& tag_buffer) {
  if (tag_buffer.empty()) {
    return {};
  }

  const std::string tag_string = ReadTag(tag_buffer.begin(), tag_buffer.end());
  LOG_IF(ERROR, tag_string.empty()) << __func__ << ": Tag not found in file.";
  return tag_string;
}

std::vector<uint8_t> ReadEntireFile(const base::FilePath& file) {
  int64_t file_size = 0;
  if (!base::GetFileSize(file, &file_size)) {
    PLOG(ERROR) << __func__ << ": Could not get file size: " << file;
    return {};
  }

  std::vector<uint8_t> contents(file_size);
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
  std::vector<uint8_t> tag(std::begin(kTagMagicUtf8), std::end(kTagMagicUtf8));
  const std::array<uint8_t, 2> tag_length =
      U16IntToBigEndian(tag_string.length());
  tag.insert(tag.end(), tag_length.begin(), tag_length.end());
  tag.insert(tag.end(), tag_string.begin(), tag_string.end());
  return tag;
}

std::string ReadTag(std::vector<uint8_t>::const_iterator begin,
                    std::vector<uint8_t>::const_iterator end) {
  const uint8_t* magic_begin = std::begin(kTagMagicUtf8);
  const uint8_t* magic_end = std::end(kTagMagicUtf8);

  std::vector<uint8_t>::const_iterator magic_str =
      std::find_end(begin, end, magic_begin, magic_end);
  if (magic_str == end) {
    return std::string();
  }

  std::vector<uint8_t>::const_iterator taglen_buf =
      internal::AdvanceIt(magic_str, magic_end - magic_begin, end);

  // Checks that the stored tag length is found within the binary.
  if (!internal::CheckRange(taglen_buf, sizeof(uint16_t), end)) {
    return std::string();
  }

  // Tag length is stored as a big-endian uint16_t.
  const uint16_t tag_len = BigEndianReadU16(taglen_buf);

  std::vector<uint8_t>::const_iterator tag_buf =
      internal::AdvanceIt(taglen_buf, sizeof(uint16_t), end);
  if (tag_buf == end) {
    return std::string();
  }

  // Checks that the specified tag is found within the binary.
  if (!internal::CheckRange(tag_buf, tag_len, end)) {
    return std::string();
  }

  return std::string(tag_buf, tag_buf + tag_len);
}

std::unique_ptr<tagging::BinaryInterface> CreateBinary(
    const base::FilePath& file,
    base::span<const uint8_t> contents) {
  if (file.MatchesExtension(FILE_PATH_LITERAL(".exe"))) {
    return CreatePEBinary(contents);
  } else if (file.MatchesExtension(FILE_PATH_LITERAL(".msi"))) {
    return CreateMSIBinary(contents);
  } else {
    std::unique_ptr<BinaryInterface> binary = CreatePEBinary(contents);
    if (!binary) {
      binary = CreateMSIBinary(contents);
    }
    return binary;
  }
}

std::string BinaryReadTagString(const base::FilePath& file) {
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
  const std::string tag_string = ReadTag(tag_data.begin(), tag_data.end());
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
    LOG(ERROR) << __func__
               << "Error while setting superfluous certificate tag.";
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

}  // namespace updater::tagging
