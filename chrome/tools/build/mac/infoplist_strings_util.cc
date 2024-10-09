// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Helper tool that is built and run during a build to pull strings from
// the GRD files and generate the InfoPlist.strings files needed for
// macOS app bundles.

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/i18n/message_formatter.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/resource/data_pack.h"

namespace {

std::unique_ptr<ui::DataPack> LoadResourceDataPack(
    const char* dir_path,
    const char* branding_strings_name,
    const std::string& locale_name) {
  auto path = base::FilePath(base::StringPrintf(
      "%s/%s_%s.pak", dir_path, branding_strings_name, locale_name.c_str()));
  path = base::MakeAbsoluteFilePath(path);
  auto resource_pack = std::make_unique<ui::DataPack>(ui::k100Percent);
  if (!resource_pack->LoadFromPath(path))
    resource_pack.reset();
  return resource_pack;
}

std::string LoadStringFromDataPack(ui::DataPack* data_pack,
                                   const std::string& data_pack_lang,
                                   uint32_t resource_id,
                                   const char* resource_id_str) {
  std::optional<std::string_view> data = data_pack->GetStringView(resource_id);
  CHECK(data.has_value()) << "failed to load string " << resource_id_str
                          << " for lang " << data_pack_lang;

  // Data pack encodes strings as either UTF8 or UTF16.
  if (data_pack->GetTextEncodingType() == ui::DataPack::UTF8)
    return std::string(data.value());
  if (data_pack->GetTextEncodingType() == ui::DataPack::UTF16) {
    return base::UTF16ToUTF8(std::u16string(
        reinterpret_cast<const char16_t*>(data->data()), data->length() / 2));
  }

  LOG(FATAL) << "requested string " << resource_id_str
             << " from binary data pack";
}

// Escape quotes, newlines, etc so there are no errors when the strings file
// is parsed.
std::string EscapeForStringsFileValue(std::string str) {
  // Since this is a build tool, we don't really worry about making this
  // the most efficient code.

  // Backslash first since we need to do it before we put in all the others
  base::ReplaceChars(str, "\\", "\\\\", &str);

  // Now the rest of them.
  base::ReplaceChars(str, "\n", "\\n", &str);
  base::ReplaceChars(str, "\r", "\\r", &str);
  base::ReplaceChars(str, "\t", "\\t", &str);
  base::ReplaceChars(str, "\"", "\\\"", &str);

  return str;
}

// The valid types for the -t arg
const char kAppType_Main[] = "main";      // Main app
const char kAppType_Helper[] = "helper";  // Helper app

}  // namespace

int main(int argc, char* const argv[]) {
  const char* version_string = nullptr;
  const char* grit_output_dir = nullptr;
  const char* branding_strings_name = nullptr;
  const char* output_dir = nullptr;
  std::string app_type = kAppType_Main;

  // Process the args
  int ch;
  while ((ch = getopt(argc, argv, "t:v:g:b:o:")) != -1) {
    switch (ch) {
      case 't':
        app_type = optarg;
        break;
      case 'v':
        version_string = optarg;
        break;
      case 'g':
        grit_output_dir = optarg;
        break;
      case 'b':
        branding_strings_name = optarg;
        break;
      case 'o':
        output_dir = optarg;
        break;
      default:
        LOG(FATAL) << "bad command line arg";
    }
  }
  argc -= optind;
  argv += optind;

  // Check our args
  CHECK(version_string) << "Missing version string";
  CHECK(grit_output_dir) << "Missing grit output dir path";
  CHECK(output_dir) << "Missing path to write InfoPlist.strings files";
  CHECK(branding_strings_name) << "Missing branding strings file name";
  CHECK(argc) << "Missing language list";
  CHECK(app_type == kAppType_Main || app_type == kAppType_Helper)
      << "Unknown app type";

  char* const* lang_list = argv;
  int lang_list_count = argc;

  base::i18n::InitializeICU();

  for (int loop = 0; loop < lang_list_count; ++loop) {
    std::string cur_lang = lang_list[loop];

    // Open the branded string pak file
    std::unique_ptr<ui::DataPack> branded_data_pack(
        LoadResourceDataPack(grit_output_dir, branding_strings_name, cur_lang));
    CHECK(branded_data_pack)
        << "failed to load branded pak for language: " << cur_lang;

    uint32_t name_id = IDS_PRODUCT_NAME;
    const char* name_id_str = "IDS_PRODUCT_NAME";
    if (app_type == kAppType_Helper) {
      name_id = IDS_HELPER_NAME;
      name_id_str = "IDS_HELPER_NAME";
    }

    // Fetch the strings.
    std::string name = LoadStringFromDataPack(branded_data_pack.get(), cur_lang,
                                              name_id, name_id_str);
    std::string copyright_format = LoadStringFromDataPack(
        branded_data_pack.get(), cur_lang, IDS_ABOUT_VERSION_COPYRIGHT,
        "IDS_ABOUT_VERSION_COPYRIGHT");

    std::string copyright =
        base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
            base::UTF8ToUTF16(copyright_format), base::Time::Now()));

    std::string permission_reason =
        LoadStringFromDataPack(branded_data_pack.get(), cur_lang,
                               IDS_RUNTIME_PERMISSION_OS_REASON_TEXT,
                               "IDS_RUNTIME_PERMISSION_OS_REASON_TEXT");

    std::string local_network_access_permission_description =
        LoadStringFromDataPack(branded_data_pack.get(), cur_lang,
                               IDS_LOCAL_NETWORK_ACCESS_PERMISSION_DESC,
                               "IDS_LOCAL_NETWORK_ACCESS_PERMISSION_DESC");

    std::string chromium_shortcut_description = LoadStringFromDataPack(
        branded_data_pack.get(), cur_lang, IDS_CHROMIUM_SHORCUT_DESCRIPTION,
        "IDS_CHROMIUM_SHORCUT_DESCRIPTION");

    // For now, assume this is ok for all languages. If we need to, this could
    // be moved into generated_resources.grd and fetched.
    std::string get_info = base::StringPrintf(
        "%s %s, %s", name.c_str(), version_string, copyright.c_str());

    // Generate the InfoPlist.strings file contents.
    std::map<std::string, std::string> infoplist_strings = {
        {"CFBundleGetInfoString", get_info},
        {"NSHumanReadableCopyright", copyright},

        {"NSBluetoothAlwaysUsageDescription", permission_reason},
        {"NSBluetoothPeripheralUsageDescription", permission_reason},
        {"NSCameraUsageDescription", permission_reason},
        {"NSLocalNetworkUsageDescription",
         local_network_access_permission_description},
        {"NSLocationUsageDescription", permission_reason},
        {"NSMicrophoneUsageDescription", permission_reason},
        {"NSWebBrowserPublicKeyCredentialUsageDescription", permission_reason},

        {"\"Chromium Shortcut\"", chromium_shortcut_description},
    };
    std::string strings_file_contents_string;
    for (const auto& kv : infoplist_strings) {
      strings_file_contents_string +=
          base::StringPrintf("%s = \"%s\";\n", kv.first.c_str(),
                             EscapeForStringsFileValue(kv.second).c_str());
    }

    // For Cocoa to find the locale at runtime, it needs to use '_' instead of
    // '-' (http://crbug.com/20441).  Also, 'en-US' should be represented
    // simply as 'en' (http://crbug.com/19165, http://crbug.com/25578).
    if (cur_lang == "en-US")
      cur_lang = "en";
    base::ReplaceChars(cur_lang, "-", "_", &cur_lang);

    // Make sure the lproj we write to exists
    std::string output_path =
        base::StringPrintf("%s/%s.lproj", output_dir, cur_lang.c_str());
    CHECK(base::CreateDirectory(base::FilePath(output_path)))
        << "failed to create '" << output_path << "'";

    // Write out the file
    // We set up Xcode projects expecting strings files to be UTF8, so make
    // sure we write the data in that form.  When Xcode copies them it will
    // put the final runtime encoding.
    output_path += "/InfoPlist.strings";
    CHECK(base::WriteFile(base::FilePath(output_path),
                          strings_file_contents_string))
        << "failed to write out '" << output_path << "'";
  }
}
