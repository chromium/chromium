// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TAG_H_
#define CHROME_UPDATER_TAG_H_

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"

namespace updater::tagging {
namespace internal {

// Advances the iterator by |distance| and makes sure that it remains valid,
// else returns |end|.
std::vector<uint8_t>::const_iterator AdvanceIt(
    std::vector<uint8_t>::const_iterator it,
    size_t distance,
    std::vector<uint8_t>::const_iterator end);

// Checks that the range [it, it + size) is found within the binary. |size| must
// be > 0.
bool CheckRange(std::vector<uint8_t>::const_iterator it,
                size_t size,
                std::vector<uint8_t>::const_iterator end);

}  // namespace internal

// Represents application requirements for admin.
enum class NeedsAdmin {
  // The application will install per user.
  kNo = 0,
  // The application will install machine-wide.
  kYes,
  // The application will install machine-wide if permissions allow, else will
  // install per-user.
  kPrefers,
};

// This struct contains the attributes for a given app parsed from a part of the
// metainstaller tag. It contains minimal policy and is intended to be a
// near-direct mapping from tag to memory. See TagArgs, which stores a list of
// these.
//
// An empty string in std::string members indicates that the given attribute did
// not appear in the tag for this app.
struct AppArgs {
  // |app_id| must not be empty and will be made lowercase.
  explicit AppArgs(std::string_view app_id);

  ~AppArgs();
  AppArgs(const AppArgs&);
  AppArgs& operator=(const AppArgs&);
  AppArgs(AppArgs&&);
  AppArgs& operator=(AppArgs&&);

  // An ASCII-encoded lowercase string. Must not be empty.
  std::string app_id;
  std::string app_name;
  std::string ap;
  std::string encoded_installer_data;
  std::string install_data_index;
  std::string experiment_labels;
  std::string untrusted_data;
  std::optional<NeedsAdmin> needs_admin;
};

std::ostream& operator<<(std::ostream&, const NeedsAdmin&);

// This struct contains the "runtime mode" parsed from the metainstaller tag.
struct RuntimeModeArgs {
  auto operator<=>(const RuntimeModeArgs&) const = default;
  std::optional<NeedsAdmin> needs_admin;
};

// This struct contains the attributes parsed from a metainstaller tag. An empty
// string in std::string members indicates that the given attribute did not
// appear in the tag.
struct TagArgs {
  // Must be kept in sync with the enum in
  // `google_update\google_update_idl.idl`. Do not include `BrowserType::kMax`
  // in the IDL file. Do not move or remove existing elements.
  enum class BrowserType {
    kUnknown = 0,
    kDefault = 1,
    kInternetExplorer = 2,
    kFirefox = 3,
    kChrome = 4,
    // Add new browsers above this.
    kMax
  };

  TagArgs();
  ~TagArgs();
  TagArgs(const TagArgs&);
  TagArgs& operator=(const TagArgs&);
  TagArgs(TagArgs&&);
  TagArgs& operator=(TagArgs&&);

  std::string bundle_name;
  std::string installation_id;
  std::string brand_code;
  std::string client_id;
  std::string experiment_labels;
  std::string referral_id;
  std::string language;
  std::optional<BrowserType> browser_type;
  std::optional<bool> flighting = false;
  std::optional<bool> usage_stats_enable;
  std::string enrollment_token;

  // List of apps to install.
  std::vector<AppArgs> apps;

  // This member is present if the "runtime mode" was provided on the command
  // line.
  std::optional<RuntimeModeArgs> runtime_mode;

  // The original tag string.
  std::string tag_string;

  // Vector of name/value attributes from the tag.
  std::vector<std::pair<std::string, std::string>> attributes;
};

std::ostream& operator<<(std::ostream&, const TagArgs::BrowserType&);

// List of possible error states that the parser can encounter.
enum class ErrorCode {
  // Parsing succeeded.
  kSuccess,

  // The attribute's name is unrecognized as an arg parameter.
  kUnrecognizedName,

  // The tag contains disallowed characters.
  // See |kDisallowedCharsInExtraArgs|.
  kTagIsInvalid,

  // All attributes require a value.
  kAttributeMustHaveValue,

  // An app attribute was specified before an app id was specified.
  kApp_AppIdNotSpecified,

  // The app's experiment label cannot be whitespace.
  kApp_ExperimentLabelsCannotBeWhitespace,

  // The specified app id is not a valid.
  // It must be ASCII-encoded and 512 bytes or less.
  kApp_AppIdIsNotValid,

  // The app name cannot be whitespace.
  kApp_AppNameCannotBeWhitespace,

  // The needsadmin value must be "yes", "no", or "prefers".
  kApp_NeedsAdminValueIsInvalid,

  // No app matches provided app id. Installer data can only be added to an app
  // that has been previously specified in the TagArgs.
  kAppInstallerData_AppIdNotFound,

  // Cannot specify installer data before specifying at least one app id.
  kAppInstallerData_InstallerDataCannotBeSpecifiedBeforeAppId,

  // The bundle name cannot be whitespace.
  kGlobal_BundleNameCannotBeWhitespace,

  // The updater experiment label cannot be whitespace.
  kGlobal_ExperimentLabelsCannotBeWhitespace,

  // The browser type specified is invalid. It must be an integer matching the
  // TagArgs::BrowserType enum.
  kGlobal_BrowserTypeIsInvalid,

  // The flighting value could not be parsed as a boolean.
  kGlobal_FlightingValueIsNotABoolean,

  // The usage stats value must be 0 or 1.
  // Note: A value of 2 is considered the same as not specifying the usage
  // stats.
  kGlobal_UsageStatsValueIsInvalid,

  // The runtime value must be "true", "persist", or "false". The values
  // "persist" and "false" are only for backward compatibility in case someone
  // uses it as an oversight, and are treated the same as "true".
  kGlobal_RuntimeModeValueIsInvalid,

  // The needsadmin value must be "yes", "no", or "prefers".
  kRuntimeMode_NeedsAdminValueIsInvalid,

  // The enrollment token must be a GUID.
  kGlobal_EnrollmentTokenValueIsInvalid,
};

std::ostream& operator<<(std::ostream&, const ErrorCode&);

// The metainstaller tag contains the metadata used to configure the updater as
// a metainstaller. This usually comes from a 3rd party source, either as
// command-line arguments or embedded in the metainstaller image after it is
// signed.
//
// The metainstaller is generic. It gets bound to a specific application by
// using this configuration.
//
// This function parses |tag| and |app_installer_data_args| into |args|.
//
// |tag| is a querystring-encoded ordered list of key-value pairs. All values
// are unescaped from url-encoding. The following keys are valid and affect the
// global parameters and have the following constraints on the value:
// - bundlename        Must not contain only whitespace.
// - iid               Can be any string.
// - brand             Can be any string.
// - client            Can be any string.
// - omahaexperiments  Must not contain only whitespace.
// - referral          Can be any string.
// - browser           Must be a positive integer greater than 0 and less than
//                     TagArgs::BrowserType::kMax.
// - lang              Can be any string.
// - flighting         Must be "true" or "false".
// - usagestats        Must be "0", "1", or "2".
// - runtime           Must be "true", "false".
// - etoken            Must be a GUID.
//
// The following keys specify app-specific attributes. "appid" must be specified
// before any other app attribute to specify the "current" app. Other app
// attributes will then affect the parameters of the most recently specified app
// ID. For example, if the tag is
// "appid=App1&brand=BrandForApp1&appid=App2&ap=ApForApp2&iid=GlobalInstallId",
// the resulting tag will look like:
//   TagArgs {
//     iid = GlobalInstallId
//     apps = [
//       AppArgs {
//         appid = App1
//         brand = BrandForApp1
//       }
//       AppArgs {
//         appid = App2
//         ap = ApForApp2
//       }
//     ]
//   }
// These attributes has the following constraints on the value:
// - appid             Can be any ASCII string. Case-insensitive.
// - ap                Can be any string.
// - experiments       Must not contain only whitespace.
// - appname           Must not contain only whitespace.
// - needsadmin        Must be "yes", "no", or "prefers".
// - installdataindex  Can by any string.
// - untrusteddata     Can be any string.
//
// |app_installer_data_args| is also a querystring-encoded ordered list of
// key-value pairs. Unlike in the |tag|, the values are no unescaped. The
// following keys are valid and affect the app installer data parameters and
// have the following constraints on the value:
// - appid        Must be a valid app id specified in |tag|.
// - installerdata  Can be any string. Must be specified after appid.
//
// Note: This method assumes all attribute names are ASCII.
ErrorCode Parse(std::string_view tag,
                std::optional<std::string_view> app_installer_data_args,
                TagArgs& args);

std::string ReadTag(std::vector<uint8_t>::const_iterator begin,
                    std::vector<uint8_t>::const_iterator end);
std::vector<uint8_t> GetTagFromTagString(const std::string& tag_string);

// Utilities for reading and writing tags to Windows PE and MSI files.
//
//
// The tag specification is as follows:
//   - The tag area begins with a magic signature 'Gact2.0Omaha'.
//   - The next 2 bytes are the tag string length in big endian.
//   - Then comes the tag string in the format "key1=value1&key2=value2".
//   - The key is alphanumeric, the value allows special characters such as '*'.
//
// A sample layout:
// +-------------------------------------+
// ~    ..............................   ~
// |    ..............................   |
// |    Other parts of the file          |
// +-------------------------------------+
// | Start of the certificate            |
// ~    ..............................   ~
// ~    ..............................   ~
// | Magic signature 'Gact2.0Omaha'      | Tag starts
// | Tag length (2 bytes in big-endian)) |
// | tag string                          |
// +-------------------------------------+
//
// A real example (an MSI file tagged with 'brand=CDCD&key2=Test'):
// +-----------------------------------------------------------------+
// |  G   a   c   t   2   .   0   O   m   a   h   a  0x0 0x14 b   r  |
// |  a   n   d   =   C   D   C   D   &   k   e   y   2   =   T   e  |
// |  s   t                                                          |
// +-----------------------------------------------------------------+
// Extracts a tag from `filename`.
std::string BinaryReadTagString(const base::FilePath& file);
std::optional<tagging::TagArgs> BinaryReadTag(const base::FilePath& file);

// Tags `file` with `tag_string` and writes the result to `file` by default, or
// to `out_file` if `out_file` is provided.
bool BinaryWriteTag(const base::FilePath& in_file,
                    const std::string& tag_string,
                    int padded_length,
                    base::FilePath out_file);

}  // namespace updater::tagging

#endif  // CHROME_UPDATER_TAG_H_
