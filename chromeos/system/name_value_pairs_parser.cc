// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/name_value_pairs_parser.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"

namespace chromeos {  // NOLINT
namespace system {

namespace {

bool GetToolOutput(int argc, const char* argv[], std::string* output) {
  DCHECK_GE(argc, 1);

  if (!base::PathExists(base::FilePath(argv[0]))) {
    LOG(WARNING) << "Tool for statistics not found: " << argv[0];
    return false;
  }

  std::vector<std::string> args;
  for (int argn = 0; argn < argc; ++argn)
    args.push_back(argv[argn]);
  if (!base::GetAppOutput(args, output)) {
    LOG(WARNING) << "Error executing " << argv[0];
    return false;
  }

  return true;
}

}  // namespace

NameValuePairsParser::NameValuePairsParser(NameValueMap* map)
    : map_(map) {
}

bool NameValuePairsParser::GetNameValuePairsFromFile(
    const base::FilePath& file_path,
    const std::string& eq,
    const std::string& delim) {
  std::string contents;
  if (base::ReadFileToString(file_path, &contents)) {
    return ParseNameValuePairs(contents, eq, delim);
  } else {
    if (base::SysInfo::IsRunningOnChromeOS())
      VLOG(1) << "Statistics file not present: " << file_path.value();
    return false;
  }
}

bool NameValuePairsParser::ParseNameValuePairsFromTool(
    int argc,
    const char* argv[],
    const std::string& eq,
    const std::string& delim,
    const std::string& comment_delim) {
  std::string output_string;
  if (!GetToolOutput(argc, argv, &output_string))
    return false;

  return ParseNameValuePairsWithComments(output_string, eq, delim,
                                         comment_delim);
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

void NameValuePairsParser::AddNameValuePair(const std::string& key,
                                            const std::string& value) {
  const auto it = map_->find(key);
  if (it == map_->end()) {
    (*map_)[key] = value;
    VLOG(1) << "name: " << key << ", value: " << value;
  } else if (it->second != value) {
    LOG(WARNING) << "Key " << key << " already has value " << it->second
                 << ", ignoring new value: " << value;
  }
}

bool NameValuePairsParser::ParseNameValuePairs(const std::string& in_string,
                                               const std::string& eq,
                                               const std::string& delim) {
  return ParseNameValuePairsWithComments(in_string, eq, delim, "");
}

bool NameValuePairsParser::ParseNameValuePairsWithComments(
    const std::string& in_string,
    const std::string& eq,
    const std::string& delim,
    const std::string& comment_delim) {
  bool all_valid = true;
  // Set up the pair tokenizer.
  base::StringTokenizer pair_toks(in_string, delim);
  pair_toks.set_quote_chars("\"");
  // Process token pairs.
  while (pair_toks.GetNext()) {
    std::string pair(pair_toks.token());
    // Anything before the first |eq| is the key, anything after is the value.
    // |eq| must exist.
    size_t eq_pos = pair.find(eq);
    if (eq_pos != std::string::npos) {
      // First |comment_delim| after |eq_pos| starts the comment.
      // A value of |std::string::npos| means that the value spans to the end
      // of |pair|.
      size_t value_size = std::string::npos;
      if (!comment_delim.empty()) {
        size_t comment_pos = pair.find(comment_delim, eq_pos + 1);
        if (comment_pos != std::string::npos)
          value_size = comment_pos - eq_pos - 1;
      }

      static const char kTrimChars[] = "\" ";
      std::string key;
      std::string value;
      base::TrimString(pair.substr(0, eq_pos), kTrimChars, &key);
      base::TrimString(pair.substr(eq_pos + 1, value_size), kTrimChars, &value);

      if (!key.empty()) {
        AddNameValuePair(key, value);
        continue;
      }
    }

    LOG(WARNING) << "Invalid token pair: " << pair << ". Ignoring.";
    all_valid = false;
  }
  return all_valid;
}

}  // namespace system
}  // namespace chromeos
