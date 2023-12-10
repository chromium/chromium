// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/tag.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/certificate_tag.h"

namespace updater {
namespace tagging {
namespace {

// The name of the bundle being installed. If not specified, the first app's
// appname is used.
constexpr base::StringPiece kTagArgBundleName = "bundlename";

// The language of the product the user is installing.
constexpr base::StringPiece kTagArgLanguage = "lang";

// Flag denoting that the user is flighting a new test feature.
constexpr base::StringPiece kTagArgFlighting = "flighting";

// Flag denoting that the user has agreed to provide usage stats, crashreports
// etc.
constexpr base::StringPiece kTagArgUsageStats = "usagestats";

// A unique value for this installation session. It can be used to follow the
// progress from the website to installation completion.
constexpr base::StringPiece kTagArgInstallationId = "iid";

// The Brand Code used for branding.
// If a brand value already exists on the system, it should be ignored.
// This value is used to set the initial brand for the updater and the client
// app.
constexpr base::StringPiece kTagArgBrandCode = "brand";

// The Client ID used for branding.
// If a client value already exists on the system, it should be ignored.
// This value is used to set the initial client for the updater and the client
// app.
constexpr base::StringPiece kTagArgClientId = "client";

// A set of experiment labels used to track installs that are included in
// experiments. Use "experiments" for per-app arguments; use "omahaexperiments"
// for updater-specific labels.
constexpr base::StringPiece kAppArgExperimentLabels = "experiments";
constexpr base::StringPiece kTagArgOmahaExperimentLabels = "omahaexperiments";

// A referral ID used for tracking referrals.
constexpr base::StringPiece kTagArgReferralId = "referral";

// Tells the updater what ap value to set in the registry.
constexpr base::StringPiece kAppArgAdditionalParameters = "ap";

// Indicates which browser to restart on successful install.
constexpr base::StringPiece kTagArgBrowserType = "browser";

// The list of arguments that are needed for a meta-installer, to
// indicate which application is being installed. These are stamped
// inside the meta-installer binary.
constexpr base::StringPiece kTagArgAppId = "appguid";
constexpr base::StringPiece kAppArgAppName = "appname";
constexpr base::StringPiece kAppArgNeedsAdmin = "needsadmin";
constexpr base::StringPiece kAppArgInstallDataIndex = "installdataindex";
constexpr base::StringPiece kAppArgUntrustedData = "untrusteddata";

// This switch allows extra data to be communicated to the application
// installer. The extra data needs to be URL-encoded. The data will be decoded
// and written to the file, that is then passed in the command line to the
// application installer in the form "/installerdata=blah.dat". One per
// application.
constexpr base::StringPiece kAppArgInstallerData = "installerdata";

// Character that is disallowed from appearing in the tag.
constexpr char kDisallowedCharInTag = '/';

// Magic string used to identify the tag in the binary.
constexpr uint8_t kTagMagicUtf8[] = {'G', 'a', 'c', 't', '2', '.',
                                     '0', 'O', 'm', 'a', 'h', 'a'};

std::optional<AppArgs::NeedsAdmin> ParseNeedsAdminEnum(base::StringPiece str) {
  if (base::EqualsCaseInsensitiveASCII("false", str))
    return AppArgs::NeedsAdmin::kNo;

  if (base::EqualsCaseInsensitiveASCII("true", str))
    return AppArgs::NeedsAdmin::kYes;

  if (base::EqualsCaseInsensitiveASCII("prefers", str))
    return AppArgs::NeedsAdmin::kPrefers;

  return std::nullopt;
}

// Returns std::nullopt if parsing failed.
std::optional<bool> ParseBool(base::StringPiece str) {
  if (base::EqualsCaseInsensitiveASCII("false", str))
    return false;

  if (base::EqualsCaseInsensitiveASCII("true", str))
    return true;

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

ErrorCode ParseBundleName(base::StringPiece value, TagArgs* args) {
  value = base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL);
  if (value.empty())
    return ErrorCode::kGlobal_BundleNameCannotBeWhitespace;

  args->bundle_name = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseInstallationId(base::StringPiece value, TagArgs* args) {
  args->installation_id = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseBrandCode(base::StringPiece value, TagArgs* args) {
  args->brand_code = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseClientId(base::StringPiece value, TagArgs* args) {
  args->client_id = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseOmahaExperimentLabels(base::StringPiece value, TagArgs* args) {
  value = base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL);
  if (value.empty())
    return ErrorCode::kGlobal_ExperimentLabelsCannotBeWhitespace;

  args->experiment_labels = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseReferralId(base::StringPiece value, TagArgs* args) {
  args->referral_id = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseBrowserType(base::StringPiece value, TagArgs* args) {
  int browser_type = 0;
  if (!base::StringToInt(value, &browser_type))
    return ErrorCode::kGlobal_BrowserTypeIsInvalid;

  if (browser_type < 0)
    return ErrorCode::kGlobal_BrowserTypeIsInvalid;

  args->browser_type =
      (browser_type < static_cast<int>(TagArgs::BrowserType::kMax))
          ? TagArgs::BrowserType(browser_type)
          : TagArgs::BrowserType::kUnknown;

  return ErrorCode::kSuccess;
}

ErrorCode ParseLanguage(base::StringPiece value, TagArgs* args) {
  // Even if we don't support the language, we want to pass it to the
  // installer. Omaha will pick its language later. See http://b/1336966.
  args->language = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseFlighting(base::StringPiece value, TagArgs* args) {
  const std::optional<bool> flighting = ParseBool(value);
  if (!flighting.has_value())
    return ErrorCode::kGlobal_FlightingValueIsNotABoolean;

  args->flighting = flighting.value();
  return ErrorCode::kSuccess;
}

ErrorCode ParseUsageStats(base::StringPiece value, TagArgs* args) {
  int tristate = 0;
  if (!base::StringToInt(value, &tristate))
    return ErrorCode::kGlobal_UsageStatsValueIsInvalid;

  if (tristate == 0) {
    args->usage_stats_enable = false;
  } else if (tristate == 1) {
    args->usage_stats_enable = true;
  } else if (tristate == 2) {
    args->usage_stats_enable = std::nullopt;
  } else {
    return ErrorCode::kGlobal_UsageStatsValueIsInvalid;
  }
  return ErrorCode::kSuccess;
}

// Parses an app ID and adds it to the list of apps in |args|, if valid.
ErrorCode ParseAppId(base::StringPiece value, TagArgs* args) {
  if (!base::IsStringASCII(value))
    return ErrorCode::kApp_AppIdIsNotValid;

  args->apps.push_back(AppArgs(value));
  return ErrorCode::kSuccess;
}

// |value| must not be empty.
// |args| must not be null.
using ParseGlobalAttributeFunPtr = ErrorCode (*)(base::StringPiece value,
                                                 TagArgs* args);

using GlobalParseTable = std::map<base::StringPiece,
                                  ParseGlobalAttributeFunPtr,
                                  CaseInsensitiveASCIICompare>;

const GlobalParseTable& GetTable() {
  static const base::NoDestructor<GlobalParseTable> instance{{
      {kTagArgBundleName, &ParseBundleName},
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
  }};
  return *instance;
}

}  // namespace global_attributes

namespace app_attributes {

ErrorCode ParseAdditionalParameters(base::StringPiece value, AppArgs* args) {
  args->ap = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseExperimentLabels(base::StringPiece value, AppArgs* args) {
  value = base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL);
  if (value.empty())
    return ErrorCode::kApp_ExperimentLabelsCannotBeWhitespace;

  args->experiment_labels = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseAppName(base::StringPiece value, AppArgs* args) {
  value = base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL);
  if (value.empty())
    return ErrorCode::kApp_AppNameCannotBeWhitespace;

  args->app_name = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseNeedsAdmin(base::StringPiece value, AppArgs* args) {
  const auto needs_admin = ParseNeedsAdminEnum(value);
  if (!needs_admin.has_value())
    return ErrorCode::kApp_NeedsAdminValueIsInvalid;

  args->needs_admin = needs_admin.value();
  return ErrorCode::kSuccess;
}

ErrorCode ParseInstallDataIndex(base::StringPiece value, AppArgs* args) {
  args->install_data_index = std::string(value);
  return ErrorCode::kSuccess;
}

ErrorCode ParseUntrustedData(base::StringPiece value, AppArgs* args) {
  args->untrusted_data = std::string(value);
  return ErrorCode::kSuccess;
}

// |value| must not be empty.
// |args| must not be null.
using ParseAppAttributeFunPtr = ErrorCode (*)(base::StringPiece value,
                                              AppArgs* args);

using AppParseTable = std::map<base::StringPiece,
                               ParseAppAttributeFunPtr,
                               CaseInsensitiveASCIICompare>;

const AppParseTable& GetTable() {
  static const base::NoDestructor<AppParseTable> instance{{
      {kAppArgAdditionalParameters, &ParseAdditionalParameters},
      {kAppArgExperimentLabels, &ParseExperimentLabels},
      {kAppArgAppName, &ParseAppName},
      {kAppArgNeedsAdmin, &ParseNeedsAdmin},
      {kAppArgInstallDataIndex, &ParseInstallDataIndex},
      {kAppArgUntrustedData, &ParseUntrustedData},
  }};
  return *instance;
}

}  // namespace app_attributes

namespace installer_data_attributes {

// Search for the given appid specified by |value| in |args->apps| and write its
// index to |current_app_index|.
ErrorCode FindAppIdInTagArgs(base::StringPiece value,
                             TagArgs* args,
                             std::optional<size_t>* current_app_index) {
  if (!base::IsStringASCII(value))
    return ErrorCode::kApp_AppIdIsNotValid;

  // Find the app in the existing list.
  for (size_t i = 0; i < args->apps.size(); i++) {
    if (base::EqualsCaseInsensitiveASCII(args->apps[i].app_id, value)) {
      *current_app_index = i;
    }
  }

  if (!current_app_index->has_value())
    return ErrorCode::kAppInstallerData_AppIdNotFound;

  return ErrorCode::kSuccess;
}

ErrorCode ParseInstallerData(base::StringPiece value,
                             TagArgs* args,
                             std::optional<size_t>* current_app_index) {
  if (!current_app_index->has_value())
    return ErrorCode::
        kAppInstallerData_InstallerDataCannotBeSpecifiedBeforeAppId;

  args->apps[current_app_index->value()].encoded_installer_data =
      std::string(value);

  return ErrorCode::kSuccess;
}

// |value| must not be empty.
// |args| must not be null.
// |current_app_index| is an in/out parameter. It stores the index of the
// current app and nullopt if no app has been set yet. Writing to it will set
// the index for future calls to these functions.
using ParseInstallerDataAttributeFunPtr =
    ErrorCode (*)(base::StringPiece value,
                  TagArgs* args,
                  std::optional<size_t>* current_app_index);

using InstallerDataParseTable = std::map<base::StringPiece,
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
std::vector<Attribute> Split(base::StringPiece query_string,
                             bool unescape_value = true) {
  std::vector<Attribute> attributes;
  for (const auto& attribute_string :
       base::SplitStringPiece(query_string, "&", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    size_t separate_pos = attribute_string.find_first_of("=");
    if (separate_pos == base::StringPiece::npos) {
      // Add a name-only attribute.
      base::StringPiece name = base::TrimWhitespaceASCII(
          attribute_string, base::TrimPositions::TRIM_ALL);
      attributes.emplace_back(std::string{name}, "");
    } else {
      base::StringPiece name =
          base::TrimWhitespaceASCII(attribute_string.substr(0, separate_pos),
                                    base::TrimPositions::TRIM_ALL);
      base::StringPiece value =
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
ErrorCode ParseTag(base::StringPiece tag, TagArgs* args) {
  const auto& global_func_lookup_table = global_attributes::GetTable();
  const auto& app_func_lookup_table = app_attributes::GetTable();

  const std::vector<std::pair<std::string, std::string>> attributes =
      query_string::Split(tag);
  for (const auto& attribute : attributes) {
    // Attribute names are only ASCII, so no i18n case folding needed.
    const base::StringPiece name = attribute.first;
    const base::StringPiece value = attribute.second;

    if (global_func_lookup_table.find(name) != global_func_lookup_table.end()) {
      if (value.empty())
        return ErrorCode::kAttributeMustHaveValue;

      const ErrorCode result = global_func_lookup_table.at(name)(value, args);
      if (result != ErrorCode::kSuccess)
        return result;
    } else if (app_func_lookup_table.find(name) !=
               app_func_lookup_table.end()) {
      if (args->apps.empty())
        return ErrorCode::kApp_AppIdNotSpecified;

      if (value.empty())
        return ErrorCode::kAttributeMustHaveValue;

      AppArgs* current_app = &args->apps.back();
      const ErrorCode result =
          app_func_lookup_table.at(name)(value, current_app);
      if (result != ErrorCode::kSuccess)
        return result;
    } else {
      return ErrorCode::kUnrecognizedName;
    }
  }

  // The bundle name inherits the first app's name, if not set.
  if (args->bundle_name.empty() && !args->apps.empty())
    args->bundle_name = args->apps[0].app_name;
  args->tag_string = tag;
  args->attributes = attributes;

  return ErrorCode::kSuccess;
}

// Parses app-specific installer data from |app_installer_data_args|.
ErrorCode ParseAppInstallerDataArgs(base::StringPiece app_installer_data_args,
                                    TagArgs* args) {
  // The currently tracked app index to apply installer data to.
  std::optional<size_t> current_app_index;

  // Installer data is assumed to be URL-encoded, so we don't unescape it.
  bool unescape_value = false;

  for (const auto& attribute :
       query_string::Split(app_installer_data_args, unescape_value)) {
    const base::StringPiece name = attribute.first;
    const base::StringPiece value = attribute.second;
    if (value.empty())
      return ErrorCode::kAttributeMustHaveValue;

    const auto& func_lookup_table = installer_data_attributes::GetTable();
    if (func_lookup_table.find(name) == func_lookup_table.end())
      return ErrorCode::kUnrecognizedName;

    const ErrorCode result =
        func_lookup_table.at(name)(value, args, &current_app_index);
    if (result != ErrorCode::kSuccess)
      return result;
  }

  return ErrorCode::kSuccess;
}

// Checks that |args| does not contain |kDisallowedCharInTag|.
bool IsValidArgs(base::StringPiece args) {
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
  constexpr size_t kMaxBufferLength = 81920;  // 80K

  base::File file(filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return {};
  }

  const int64_t file_length = file.GetLength();

  int bytes_to_read = kMaxBufferLength;
  int64_t offset = 0;
  if (file_length > static_cast<int64_t>(bytes_to_read)) {
    offset = file_length - bytes_to_read;
  } else {
    bytes_to_read = file_length;
  }

  std::vector<uint8_t> buffer(bytes_to_read + 1);
  const int num_bytes_read =
      file.Read(offset, reinterpret_cast<char*>(&buffer[0]), bytes_to_read);
  if (num_bytes_read != bytes_to_read) {
    return {};
  }

  return buffer;
}

std::optional<tagging::TagArgs> ParseTagBuffer(
    const std::vector<uint8_t>& tag_buffer) {
  if (tag_buffer.empty()) {
    return {};
  }

  const std::string tag_string = ReadTag(tag_buffer.begin(), tag_buffer.end());
  if (tag_string.empty()) {
    LOG(ERROR) << __func__ << ": Tag not found in file.";
    return {};
  }

  tagging::TagArgs tag_args;
  const tagging::ErrorCode error = tagging::Parse(tag_string, {}, &tag_args);
  if (error != tagging::ErrorCode::kSuccess) {
    LOG(ERROR) << __func__ << ": Invalid tag string: " << tag_string << ": "
               << error;
    return {};
  }
  return tag_args;
}

std::vector<uint8_t> ReadEntireFile(const base::FilePath& file) {
  int64_t file_size = 0;
  if (!base::GetFileSize(file, &file_size)) {
    LOG(ERROR) << __func__ << ": Could not get file size: " << file << ": "
               << logging::GetLastSystemErrorCode();
    return {};
  }

  std::vector<uint8_t> contents(file_size);
  if (base::ReadFile(file, reinterpret_cast<char*>(&contents.front()),
                     contents.size()) == -1) {
    LOG(ERROR) << __func__ << ": Could not read file: " << file << ": "
               << logging::GetLastSystemErrorCode();
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

AppArgs::AppArgs(base::StringPiece app_id)
    : app_id(base::ToLowerASCII(app_id)) {
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

ErrorCode Parse(base::StringPiece tag,
                std::optional<base::StringPiece> app_installer_data_args,
                TagArgs* args) {
  if (!IsValidArgs(tag))
    return ErrorCode::kTagIsInvalid;

  const ErrorCode result = ParseTag(tag, args);
  if (result != ErrorCode::kSuccess)
    return result;

  if (!app_installer_data_args.has_value())
    return ErrorCode::kSuccess;

  if (!IsValidArgs(app_installer_data_args.value()))
    return ErrorCode::kTagIsInvalid;

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
  }
}

std::ostream& operator<<(std::ostream& os,
                         const AppArgs::NeedsAdmin& needs_admin) {
  switch (needs_admin) {
    case AppArgs::NeedsAdmin::kNo:
      return os << "AppArgs::NeedsAdmin::kNo";
    case AppArgs::NeedsAdmin::kYes:
      return os << "AppArgs::NeedsAdmin::kYes";
    case AppArgs::NeedsAdmin::kPrefers:
      return os << "AppArgs::NeedsAdmin::kPrefers";
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
      std::search(begin, end, magic_begin, magic_end);
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

std::string ExeReadTag(const base::FilePath& file) {
  const std::vector<uint8_t> contents = ReadEntireFile(file);
  std::optional<tagging::Binary> bin = Binary::Parse(contents);
  if (!bin) {
    LOG(ERROR) << __func__ << ": Could not parse binary: " << file;
    return {};
  }

  std::optional<base::span<const uint8_t>> tag = bin->tag();
  if (!tag) {
    LOG(ERROR) << __func__ << ": No superfluous certificate in file: " << file;
    return {};
  }

  const std::vector<const uint8_t> tag_data = {tag->begin(), tag->end()};
  const std::string tag_string = ReadTag(tag_data.begin(), tag_data.end());
  if (tag_string.empty()) {
    LOG(ERROR) << __func__ << ": file is untagged: " << file;
  }
  return tag_string;
}

bool ExeWriteTag(const base::FilePath& in_file,
                 const std::string& tag_string,
                 int padded_length,
                 const base::FilePath& out_file) {
  const std::vector<uint8_t> contents = ReadEntireFile(in_file);
  std::optional<tagging::Binary> bin = tagging::Binary::Parse(contents);
  if (!bin) {
    LOG(ERROR) << __func__ << ": Could not parse binary: " << in_file;
    return false;
  }

  // Validate the tag string, if any.
  if (!tag_string.empty()) {
    tagging::TagArgs tag_args;
    const tagging::ErrorCode error = tagging::Parse(tag_string, {}, &tag_args);
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
  if (!base::WriteFile(out_file, *new_contents)) {
    LOG(ERROR) << __func__ << "Error while writing updated file: " << out_file
               << ": " << logging::GetLastSystemErrorCode();
    return false;
  }
  return true;
}

std::optional<tagging::TagArgs> MsiReadTag(const base::FilePath& filename) {
  return ParseTagBuffer(ReadFileTail(filename));
}

bool MsiWriteTag(const base::FilePath& file,
                 const std::string& tag_string,
                 base::FilePath out_file) {
  if (tag_string.empty()) {
    LOG(ERROR) << __func__ << ": empty tag string.";
    return false;
  }

  // Check if the file is already tagged.
  if (MsiReadTag(file)) {
    LOG(ERROR) << __func__ << ": file already tagged: " << file;
    return false;
  }

  // Validate the tag string.
  tagging::TagArgs tag_args;
  const tagging::ErrorCode error = tagging::Parse(tag_string, {}, &tag_args);
  if (error != tagging::ErrorCode::kSuccess) {
    LOG(ERROR) << __func__ << ": Invalid tag string: " << tag_string << ": "
               << error;
    return false;
  }

  if (out_file.empty()) {
    out_file = file;
  } else if (!base::CopyFile(file, out_file)) {
    return false;
  }
  base::File out(out_file, base::File::FLAG_OPEN | base::File::FLAG_APPEND);
  if (!out.IsValid()) {
    return false;
  }

  const std::vector<uint8_t> tag = GetTagFromTagString(tag_string);
  return out.WriteAtCurrentPos(reinterpret_cast<const char*>(tag.data()),
                               tag.size()) == static_cast<int>(tag.size());
}

}  // namespace tagging
}  // namespace updater
