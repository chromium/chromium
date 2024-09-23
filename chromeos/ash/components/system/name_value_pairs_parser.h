// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"

namespace base {
class FilePath;
}

namespace ash::system {

// The name value pairs formats the parser understands.
enum class NameValuePairsFormat {
  // Values produced by the VPD filtering tool.
  // Each key and value is surrounded by double quotes ('"'), and separated by
  // an equal ('=') sign. There is no whitespace around the quoted key or value.
  kVpdDump,
  // Values produced by write-machine-info in the machine-info file.
  // The base for this format is that of |kVpdDump|, with the additional
  // provision that keys may be unquoted.
  kMachineInfo,
  // Values produced by the crossystem tool.
  // Each key and value is unquoted, and separated by an equal ('=') sign.
  // Whitespace is allowed, and used, around key and value, and is not part of
  // either. Comments are supported and start with a sharp ('#') character and
  // run to the end of the line.
  kCrossystem
};

// The parser is used to get machine info as name-value pairs. Defined here to
// be accessible by tests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM) NameValuePairsParser {
 public:
  using NameValueMap = base::flat_map<std::string, std::string>;

  // The obtained info will be written into the given map.
  explicit NameValuePairsParser(NameValueMap* map);

  ~NameValuePairsParser();

  NameValuePairsParser(const NameValuePairsParser&) = delete;
  NameValuePairsParser& operator=(const NameValuePairsParser&) = delete;

  // Parses name-value pairs in the specified |format| from a file.
  //
  // Returns false if there was any error when parsing the file. Valid pairs
  // will still be added to the map.
  bool ParseNameValuePairsFromFile(const base::FilePath& file_path,
                                   NameValuePairsFormat format);

  // Parses name-value pairs in the specified |format| from the standard output
  // of a tool invocation specified by |command|.
  //
  // Returns false if there was any error in the command invocation or when
  // parsing its output. Valid pairs will still be added to the map.
  bool ParseNameValuePairsFromTool(const base::CommandLine& command,
                                   NameValuePairsFormat format);

  // Parses name-value pairs in the specified |format| from a string.
  //
  // Returns false if there was any error in the command invocation or when
  // parsing its output. Valid pairs will still be added to the map.
  bool ParseNameValuePairsFromString(const std::string& string,
                                     NameValuePairsFormat format);

  // Delete all pairs with |value|.
  void DeletePairsWithValue(const std::string& value);

 private:
  FRIEND_TEST_ALL_PREFIXES(VpdDumpNameValuePairsParserTest,
                           TestParseNameValuePairs);
  FRIEND_TEST_ALL_PREFIXES(NameValuePairsParser,
                           TestParseNameValuePairsInVpdDumpFormat);
  FRIEND_TEST_ALL_PREFIXES(NameValuePairsParser, TestParseErrorInVpdDumpFormat);
  FRIEND_TEST_ALL_PREFIXES(NameValuePairsParser,
                           TestParseNameValuePairsInMachineInfoFormat);
  FRIEND_TEST_ALL_PREFIXES(NameValuePairsParser,
                           TestParseNameValuePairsFromCrossytemTool);

  friend class NameValuePairsParserFuzzer;

  void AddNameValuePair(const std::string& key, const std::string& value);

  bool ParseNameValuePairs(const std::string& input,
                           NameValuePairsFormat format);

  raw_ptr<NameValueMap> map_;
};

}  // namespace ash::system

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_
