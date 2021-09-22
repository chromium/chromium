// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_
#define CHROMEOS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"

namespace base {
class FilePath;
}

namespace chromeos {
namespace system {

// The parser is used to get machine info as name-value pairs. Defined here to
// be accessible by tests.
class COMPONENT_EXPORT(CHROMEOS_SYSTEM) NameValuePairsParser {
 public:
  typedef std::map<std::string, std::string> NameValueMap;

  // The obtained info will be written into the given map.
  explicit NameValuePairsParser(NameValueMap* map);

  // Parses name-value pairs from the file.
  // Returns false if there was any error in the file. Valid pairs will still be
  // added to the map.
  bool GetNameValuePairsFromFile(const base::FilePath& file_path,
                                 const std::string& eq,
                                 const std::string& delim);

  // Same as ParseNameValuePairsWithComments(), but uses the output of the given
  // tool as the input to parse.
  bool ParseNameValuePairsFromTool(int argc,
                                   const char* argv[],
                                   const std::string& eq,
                                   const std::string& delim,
                                   const std::string& comment_delim);

  // Delete all pairs with |value|.
  void DeletePairsWithValue(const std::string& value);

 private:
  FRIEND_TEST_ALL_PREFIXES(NameValuePairsParser, TestParseNameValuePairs);
  FRIEND_TEST_ALL_PREFIXES(NameValuePairsParser,
                           TestParseNameValuePairsWithComments);

  void AddNameValuePair(const std::string& key, const std::string& value);

  // These will parse strings with output in the format:
  // <key><EQ><value><DELIM>[<key><EQ><value>][...]
  // e.g. ParseNameValuePairs("key1=value1 key2=value2", "=", " ")
  // Returns false if there was any error in in_string. Valid pairs will still
  // be added to the map.
  bool ParseNameValuePairs(const std::string& in_string,
                           const std::string& eq,
                           const std::string& delim);

  // This version allows for values which end with a comment beginning with
  // |comment_delim|.
  // e.g. "key2=value2 # Explanation of value\n"
  // Returns false if there was any error in in_string. Valid pairs will still
  // be added to the map.
  bool ParseNameValuePairsWithComments(const std::string& in_string,
                                       const std::string& eq,
                                       const std::string& delim,
                                       const std::string& comment_delim);

  NameValueMap* map_;

  DISALLOW_COPY_AND_ASSIGN(NameValuePairsParser);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_NAME_VALUE_PAIRS_PARSER_H_
