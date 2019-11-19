// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter_util.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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

// Maximum length of an Android property value.
constexpr int kAndroidMaxPropertyLength = 91;

// The following 4 functions as well as the constants above are the _exact_ copy
// of the ones in platform2/arc/setup/arc_setup_util.cc. Do not modify the
// implementation directly here. Instead, modify it in platform2 with proper
// unit tests, then roll the change into Chromium.
// TODO(yusukes): Once we stop supporting the container, move the unit tests
// to Chromium and delete platform2/arc/setup/.

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

bool ExpandPropertyContents(const std::string& content,
                            brillo::CrosConfigInterface* config,
                            std::string* expanded_content) {
  const std::vector<std::string> lines = base::SplitString(
      content, "\n", base::WhitespaceHandling::KEEP_WHITESPACE,
      base::SplitResult::SPLIT_WANT_ALL);

  std::string new_properties;
  for (std::string line : lines) {
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

  *expanded_content = new_properties;
  return true;
}

}  // namespace

CrosConfig::CrosConfig() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(chromeos::switches::kArcBuildProperties)) {
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

bool ExpandPropertyFile(const base::FilePath& input,
                        const base::FilePath& output,
                        CrosConfig* config) {
  std::string content;
  std::string expanded;
  if (!base::ReadFileToString(input, &content)) {
    PLOG(ERROR) << "Failed to read " << input;
    return false;
  }
  if (!ExpandPropertyContents(content, config, &expanded))
    return false;
  if (base::WriteFile(output, expanded.data(), expanded.size()) !=
      static_cast<int>(expanded.size())) {
    PLOG(ERROR) << "Failed to write to " << output;
    return false;
  }
  return true;
}

}  // namespace arc
