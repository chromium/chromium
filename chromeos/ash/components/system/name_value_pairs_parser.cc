// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/name_value_pairs_parser.h"

#include <stddef.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"

namespace ash::system {

namespace {

// Runs a tool and capture its standard output into |output|. Returns false
// if the tool cannot be run.
bool GetToolOutput(const base::CommandLine& command, std::string* output) {
  if (!base::PathExists(command.GetProgram())) {
    LOG(WARNING) << "Tool for statistics not found: " << command.GetProgram();
    return false;
  }
  if (!base::GetAppOutput(command, output)) {
    LOG(WARNING) << "Error executing " << command.GetProgram();
    return false;
  }

  return true;
}

// Assigns a non quoted version of |input| to |unquoted|, and returns
// whether |input| was actually quoted or not.
bool GetUnquotedString(const std::string& input, std::string* unquoted) {
  const size_t input_size = input.size();

  if (input_size >= 2 && input[0] == '"' && input[input_size - 1] == '"')
    unquoted->assign(input, 1, input_size - 2);
  else
    unquoted->assign(input);

  return unquoted->size() != input_size;
}

// Assigns a non commented version of |input| to |uncommented|. Whitespace
// before the comment is trimmed.
void GetUncommentedString(const std::string& input, std::string* uncommented) {
  const size_t comment_pos = input.find('#');
  if (comment_pos == std::string::npos) {
    uncommented->assign(input);
  } else {
    uncommented->assign(input, 0, comment_pos);
    base::TrimWhitespaceASCII(*uncommented, base::TRIM_TRAILING, uncommented);
  }
}

// Parse a name from |input|, validating that it is in |format|, and assign it
// to |name|.
bool ParseName(const std::string& input,
               NameValuePairsFormat format,
               std::string* name) {
  bool parsed_ok = false;
  switch (format) {
    case NameValuePairsFormat::kVpdDump:
      // The name must be quoted, and we unquote it.
      parsed_ok = GetUnquotedString(input, name);
      break;
    case NameValuePairsFormat::kMachineInfo:
      // The name may be quoted, and we unquote it.
      GetUnquotedString(input, name);
      parsed_ok = true;
      break;
    case NameValuePairsFormat::kCrossystem:
      // We trim all ASCII whitespace and the name then must not be quoted.
      base::TrimWhitespaceASCII(input, base::TRIM_ALL, name);
      parsed_ok = !GetUnquotedString(*name, name);
      break;
  }

  // Names must not be empty in addition to having parsed successfully.
  return parsed_ok && !name->empty();
}

// Parse a value from |input|, validating that it is in |format|, and assign it
// to |name|.
bool ParseValue(const std::string& input,
                NameValuePairsFormat format,
                std::string* value) {
  if (format == NameValuePairsFormat::kCrossystem) {
    // The crossystem format allows for comments, remove them.
    GetUncommentedString(input, value);
    // We trim all ASCII whitespace and preserve the rest as is.
    base::TrimWhitespaceASCII(*value, base::TRIM_ALL, value);
    return true;
  } else {
    // The value must be quoted, and we unquote it.
    return GetUnquotedString(input, value);
  }
}

// Return a string for logging a value.
std::string GetLoggingStringForValue(const std::string& value) {
  return "value: " + value;
}

const char* GetNameValuePairsFormatName(NameValuePairsFormat format) {
  switch (format) {
    case NameValuePairsFormat::kVpdDump:
      return "VPD dump";
    case NameValuePairsFormat::kMachineInfo:
      return "machine info";
    case NameValuePairsFormat::kCrossystem:
      return "crossystem";
  }
  return "unknown";
}

}  // namespace

NameValuePairsParser::NameValuePairsParser(NameValueMap* map)
    : map_(map) {
}

NameValuePairsParser::~NameValuePairsParser() = default;

bool NameValuePairsParser::ParseNameValuePairsFromFile(
    const base::FilePath& file_path,
    NameValuePairsFormat format) {
  std::string file_contents;
  if (base::ReadFileToString(file_path, &file_contents)) {
    return ParseNameValuePairs(file_contents, format);
  } else {
    if (base::SysInfo::IsRunningOnChromeOS())
      VLOG(1) << "Statistics file not present: " << file_path.value();
    return false;
  }
}

bool NameValuePairsParser::ParseNameValuePairsFromTool(
    const base::CommandLine& command,
    NameValuePairsFormat format) {
  std::string output_string;
  if (!GetToolOutput(command, &output_string))
    return false;

  return ParseNameValuePairs(output_string, format);
}

bool NameValuePairsParser::ParseNameValuePairsFromString(
    const std::string& input,
    NameValuePairsFormat format) {
  return ParseNameValuePairs(input, format);
}

void NameValuePairsParser::DeletePairsWithValue(const std::string& value) {
  auto it = map_->begin();
  while (it != map_->end()) {
    if (it->second == value) {
      it = map_->erase(it);
    } else {
      it++;
    }
  }
}

void NameValuePairsParser::AddNameValuePair(const std::string& name,
                                            const std::string& value) {
  const auto it = map_->find(name);
  if (it == map_->end()) {
    (*map_)[name] = value;
    VLOG(1) << "Name: " << name << ", " << GetLoggingStringForValue(value);
  } else if (it->second != value) {
    LOG(WARNING) << "Name: " << name << " already has "
                 << GetLoggingStringForValue(it->second) << ", ignoring new "
                 << GetLoggingStringForValue(value);
  }
}

bool NameValuePairsParser::ParseNameValuePairs(const std::string& input,
                                               NameValuePairsFormat format) {
  bool all_valid = true;

  // We use StringPairs to parse pairs since this is the class that is also
  // used to parse machine-info for server backed state keys in Chromium OS.
  base::StringPairs pairs;
  // This gives us somewhat more lenient parsing than strictly respecting the
  // formats we want. For example, whitespace will be removed around the equal
  // sign regardless of format. That's okay as we care more about consistency
  // with the server backed state keys parser than the strictness of the format,
  // and we still preserve our ability to handle the quoted values as we wish.
  base::SplitStringIntoKeyValuePairs(input, '=', '\n', &pairs);

  for (const auto& pair : pairs) {
    std::string name;
    if (!ParseName(pair.first, format, &name)) {
      LOG(WARNING) << "Could not parse " << GetNameValuePairsFormatName(format)
                   << " name-value name from: " << pair.first;
      all_valid = false;
      continue;
    }

    std::string value;
    if (!ParseValue(pair.second, format, &value)) {
      LOG(WARNING) << "Could not parse " << GetNameValuePairsFormatName(format)
                   << " name-value value from: " << pair.second;
      all_valid = false;
      continue;
    }

    AddNameValuePair(name, value);
  }

  return all_valid;
}

}  // namespace ash::system
