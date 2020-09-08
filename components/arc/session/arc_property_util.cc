// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_property_util.h"

#include <algorithm>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chromeos/constants/chromeos_switches.h"

namespace brillo {
// This is to use the platform2 code without any modifications.
using CrosConfigInterface = arc::CrosConfig;
}  // namespace brillo

namespace arc {
namespace {

// The path in the chromeos-config database where Android properties will be
// looked up.
constexpr char kCrosConfigPropertiesPath[] = "/arc/build-properties";

// Android property name used to store the board name.
constexpr char kBoardPropertyPrefix[] = "ro.product.board=";

// Android property name for custom key used for Play Auto Install selection.
constexpr char kOEMKey1PropertyPrefix[] = "ro.oem.key1=";

// Configuration property name of an optional string that contains a comma-
// separated list of regions to include in the OEM key property.
constexpr char kPAIRegionsPropertyName[] = "pai-regions";

// Properties related to dynamically adding native bridge 64 bit support.
// Note that "%s" is lated replaced with a partition name which is either an
// empty string or a string that ends with '.' e.g. "system_ext.".
constexpr char kAbilistPropertyPrefixTemplate[] = "ro.%sproduct.cpu.abilist=";
constexpr char kAbilistPropertyExpected[] = "x86_64,x86,armeabi-v7a,armeabi";
constexpr char kAbilistPropertyReplacement[] =
    "x86_64,x86,arm64-v8a,armeabi-v7a,armeabi";
constexpr char kAbilist64PropertyPrefixTemplate[] =
    "ro.%sproduct.cpu.abilist64=";
constexpr char kAbilist64PropertyExpected[] = "x86_64";
constexpr char kAbilist64PropertyReplacement[] = "x86_64,arm64-v8a";
constexpr char kDalvikVmIsaArm64[] = "ro.dalvik.vm.isa.arm64=x86_64";

// Maximum length of an Android property value.
constexpr int kAndroidMaxPropertyLength = 91;

bool FindProperty(const std::string& line_prefix_to_find,
                  std::string* out_prop,
                  const std::string& line) {
  if (base::StartsWith(line, line_prefix_to_find,
                       base::CompareCase::SENSITIVE)) {
    *out_prop = line.substr(line_prefix_to_find.length());
    return true;
  }
  return false;
}

bool TruncateAndroidProperty(const std::string& line, std::string* truncated) {
  // If line looks like key=value, cut value down to the max length of an
  // Android property.  Build fingerprint needs special handling to preserve the
  // trailing dev-keys indicator, but other properties can just be truncated.
  size_t eq_pos = line.find('=');
  if (eq_pos == std::string::npos) {
    *truncated = line;
    return true;
  }

  std::string val = line.substr(eq_pos + 1);
  base::TrimWhitespaceASCII(val, base::TRIM_ALL, &val);
  if (val.length() <= kAndroidMaxPropertyLength) {
    *truncated = line;
    return true;
  }

  const std::string key = line.substr(0, eq_pos);
  LOG(WARNING) << "Truncating property " << key << " value: " << val;
  if (key == "ro.bootimage.build.fingerprint" &&
      base::EndsWith(val, "/dev-keys", base::CompareCase::SENSITIVE)) {
    // Typical format is brand/product/device/.../dev-keys.  We want to remove
    // characters from product and device to get below the length limit.
    // Assume device has the format {product}_cheets.
    std::vector<std::string> fields =
        base::SplitString(val, "/", base::WhitespaceHandling::KEEP_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_ALL);
    if (fields.size() < 5) {
      LOG(ERROR) << "Invalid build fingerprint: " << val;
      return false;
    }

    size_t remove_chars = (val.length() - kAndroidMaxPropertyLength + 1) / 2;
    if (fields[1].length() <= remove_chars) {
      LOG(ERROR) << "Unable to remove " << remove_chars << " characters from "
                 << fields[1];
      return false;
    }
    fields[1] = fields[1].substr(0, fields[1].length() - remove_chars);
    fields[2] = fields[1] + "_cheets";
    val = base::JoinString(fields, "/");
  } else {
    val = val.substr(0, kAndroidMaxPropertyLength);
  }

  *truncated = key + "=" + val;
  return true;
}

// Computes the value of ro.oem.key1 based on the build-time ro.product.board
// value and the device's region of origin.
std::string ComputeOEMKey(brillo::CrosConfigInterface* config,
                          const std::string& board) {
  std::string regions;
  if (!config->GetString(kCrosConfigPropertiesPath, kPAIRegionsPropertyName,
                         &regions)) {
    // No region list found, just use the board name as before.
    return board;
  }

  std::string region_code;
  if (!base::GetAppOutput({"cros_region_data", "region_code"}, &region_code)) {
    LOG(WARNING) << "Failed to get region code";
    return board;
  }

  // Remove trailing newline.
  region_code.erase(std::remove(region_code.begin(), region_code.end(), '\n'),
                    region_code.end());

  // Allow wildcard configuration to indicate that all regions should be
  // included.
  if (regions.compare("*") == 0 && region_code.length() >= 2)
    return board + "_" + region_code;

  // Check to see if region code is in the list of regions that should be
  // included in the property.
  std::vector<std::string> region_vector =
      base::SplitString(regions, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (const auto& region : region_vector) {
    if (region_code.compare(region) == 0)
      return board + "_" + region_code;
  }

  return board;
}

bool IsComment(const std::string& line) {
  return base::StartsWith(
      base::TrimWhitespaceASCII(line, base::TrimPositions::TRIM_LEADING), "#",
      base::CompareCase::SENSITIVE);
}

bool ExpandPropertyContents(const std::string& content,
                            brillo::CrosConfigInterface* config,
                            std::string* expanded_content,
                            bool filter_non_ro_props,
                            bool add_native_bridge_64bit_support,
                            bool append_dalvik_isa,
                            const std::string& partition_name) {
  const std::vector<std::string> lines = base::SplitString(
      content, "\n", base::WhitespaceHandling::KEEP_WHITESPACE,
      base::SplitResult::SPLIT_WANT_ALL);

  std::string new_properties;
  for (std::string line : lines) {
    // Since Chrome only expands ro. properties at runtime, skip processing
    // non-ro lines here for R+. For P, we cannot do that because the
    // expanded property files will directly replace the original ones via
    // bind mounts.
    if (filter_non_ro_props &&
        !base::StartsWith(line, "ro.", base::CompareCase::SENSITIVE)) {
      if (!IsComment(line) && line.find('{') != std::string::npos) {
        // The non-ro property has substitution(s).
        LOG(ERROR) << "Found substitution(s) in a non-ro property: " << line;
        return false;
      }
      continue;
    }

    // First expand {property} substitutions in the string.  The insertions
    // may contain substitutions of their own, so we need to repeat until
    // nothing more is found.
    bool inserted;
    do {
      inserted = false;
      size_t match_start = line.find('{');
      size_t prev_match = 0;  // 1 char past the end of the previous {} match.
      std::string expanded;
      // Find all of the {} matches on the line.
      while (match_start != std::string::npos) {
        expanded += line.substr(prev_match, match_start - prev_match);

        size_t match_end = line.find('}', match_start);
        if (match_end == std::string::npos) {
          LOG(ERROR) << "Unmatched { found in line: " << line;
          return false;
        }

        const std::string keyword =
            line.substr(match_start + 1, match_end - match_start - 1);
        std::string replacement;
        if (config->GetString(kCrosConfigPropertiesPath, keyword,
                              &replacement)) {
          expanded += replacement;
          inserted = true;
        } else {
          LOG(ERROR) << "Did not find a value for " << keyword
                     << " while expanding " << line;
          return false;
        }

        prev_match = match_end + 1;
        match_start = line.find('{', match_end);
      }
      if (prev_match != std::string::npos)
        expanded += line.substr(prev_match);
      line = expanded;
    } while (inserted);

    if (add_native_bridge_64bit_support) {
      // Special-case ro.<partition>product.cpu.abilist and
      // ro.<partition>product.cpu.abilist64 to add ARM64.
      // Note that <partition> is either an empty string or a string that ends
      // with '.' e.g. "system_ext.".
      std::string prefix = base::StringPrintf(kAbilistPropertyPrefixTemplate,
                                              partition_name.c_str());
      std::string value;
      if (FindProperty(prefix, &value, line)) {
        if (value == kAbilistPropertyExpected) {
          line = prefix + std::string(kAbilistPropertyReplacement);
        } else {
          LOG(ERROR) << "Found unexpected value for " << prefix << ", value "
                     << value;
          return false;
        }
      }
      prefix = base::StringPrintf(kAbilist64PropertyPrefixTemplate,
                                  partition_name.c_str());
      if (FindProperty(prefix, &value, line)) {
        if (value == kAbilist64PropertyExpected) {
          line = prefix + std::string(kAbilist64PropertyReplacement);
        } else {
          LOG(ERROR) << "Found unexpected value for " << prefix << ", value "
                     << value;
          return false;
        }
      }
    }

    std::string truncated;
    if (!TruncateAndroidProperty(line, &truncated)) {
      LOG(ERROR) << "Unable to truncate property: " << line;
      return false;
    }

    new_properties += truncated + "\n";

    // Special-case ro.product.board to compute ro.oem.key1 at runtime, as it
    // can depend upon the device region.
    std::string property;
    if (FindProperty(kBoardPropertyPrefix, &property, line)) {
      std::string oem_key_property = ComputeOEMKey(config, property);
      new_properties +=
          std::string(kOEMKey1PropertyPrefix) + oem_key_property + "\n";
    }
  }

  if (append_dalvik_isa) {
    // Special-case to add ro.dalvik.vm.isa.arm64.
    new_properties += std::string(kDalvikVmIsaArm64) + "\n";
  }

  *expanded_content = new_properties;
  return true;
}

bool ExpandPropertyFile(const base::FilePath& input,
                        const base::FilePath& output,
                        CrosConfig* config,
                        bool append,
                        bool add_native_bridge_64bit_support,
                        bool append_dalvik_isa,
                        const std::string& partition_name) {
  std::string content;
  std::string expanded;
  if (!base::ReadFileToString(input, &content)) {
    if (base::SysInfo::IsRunningOnChromeOS())
      PLOG(ERROR) << "Failed to read " << input;
    return false;
  }
  if (!ExpandPropertyContents(content, config, &expanded,
                              /*filter_non_ro_props=*/append,
                              add_native_bridge_64bit_support,
                              append_dalvik_isa, partition_name))
    return false;
  if (append && base::PathExists(output)) {
    if (!base::AppendToFile(output, expanded.data(), expanded.size())) {
      PLOG(ERROR) << "Failed to append to " << output;
      return false;
    }
  } else {
    if (base::WriteFile(output, expanded.data(), expanded.size()) !=
        static_cast<int>(expanded.size())) {
      PLOG(ERROR) << "Failed to write to " << output;
      return false;
    }
  }
  return true;
}

}  // namespace

CrosConfig::CrosConfig() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(chromeos::switches::kArcBuildProperties)) {
    if (base::SysInfo::IsRunningOnChromeOS())
      LOG(ERROR) << chromeos::switches::kArcBuildProperties << " is not found";
    return;
  }
  std::string command_line_value =
      cl->GetSwitchValueASCII(chromeos::switches::kArcBuildProperties);
  info_ = base::JSONReader::Read(command_line_value);
  if (!info_) {
    LOG(ERROR) << "JSONReader failed reading ARC build properties: "
               << command_line_value;
    return;
  }
  if (!info_->is_dict()) {
    LOG(ERROR) << chromeos::switches::kArcBuildProperties
               << " is not a dictionary";
    info_.reset();
    return;
  }
}

CrosConfig::~CrosConfig() = default;

bool CrosConfig::GetString(const std::string& path,
                           const std::string& property,
                           std::string* val_out) {
  if (path != kCrosConfigPropertiesPath)
    return false;
  if (!info_)
    return false;
  const std::string* value = info_->FindStringKey(property);
  if (!value)
    return false;
  *val_out = *value;
  return true;
}

bool ExpandPropertyContentsForTesting(const std::string& content,
                                      brillo::CrosConfigInterface* config,
                                      std::string* expanded_content) {
  return ExpandPropertyContents(content, config, expanded_content,
                                /*filter_non_ro_props=*/true,
                                /*add_native_bridge_64bit_support=*/false,
                                false, std::string());
}

bool TruncateAndroidPropertyForTesting(const std::string& line,
                                       std::string* truncated) {
  return TruncateAndroidProperty(line, truncated);
}

bool ExpandPropertyFileForTesting(const base::FilePath& input,
                                  const base::FilePath& output,
                                  CrosConfig* config) {
  return ExpandPropertyFile(input, output, config, /*append=*/false,
                            /*add_native_bridge_64bit_support=*/false, false,
                            std::string());
}

bool ExpandPropertyFiles(const base::FilePath& source_path,
                         const base::FilePath& dest_path,
                         bool single_file,
                         bool add_native_bridge_64bit_support) {
  CrosConfig config;
  if (single_file)
    base::DeleteFile(dest_path);

  // default.prop may not exist. Silently skip it if not found.
  for (const auto& tuple :
       // The order has to match the one in PropertyLoadBootDefaults() in
       // system/core/init/property_service.cpp.
       // Note: Our vendor image doesn't have /vendor/default.prop although
       // PropertyLoadBootDefaults() tries to open it.
       {std::tuple<const char*, bool, bool, const char*>{"default.prop", true,
                                                         false, ""},
        {"build.prop", false, true, ""},
        {"system_ext_build.prop", true, false, "system_ext."},
        {"vendor_build.prop", false, false, "vendor."},
        {"odm_build.prop", true, false, "odm."},
        {"product_build.prop", true, false, "product."}}) {
    const char* file = std::get<0>(tuple);
    const bool is_optional = std::get<1>(tuple);
    // When true, unconditionally add |kDalvikVmIsaArm64| property.
    const bool append_dalvik_isa =
        std::get<2>(tuple) && add_native_bridge_64bit_support;
    // Search for ro.<partition_name>product.cpu.abilist* properties.
    const char* partition_name = std::get<3>(tuple);

    if (is_optional && !base::PathExists(source_path.Append(file)))
      continue;

    if (!ExpandPropertyFile(
            source_path.Append(file),
            single_file ? dest_path : dest_path.Append(file), &config,
            /*append=*/single_file, add_native_bridge_64bit_support,
            append_dalvik_isa, partition_name)) {
      if (base::SysInfo::IsRunningOnChromeOS())
        LOG(ERROR) << "Failed to expand " << source_path.Append(file);
      return false;
    }
  }
  return true;
}

}  // namespace arc
