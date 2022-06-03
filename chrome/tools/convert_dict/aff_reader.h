// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_CONVERT_DICT_AFF_READER_H__
#define CHROME_TOOLS_CONVERT_DICT_AFF_READER_H__

#include <map>
#include <stdio.h>
#include <string>
#include <vector>

namespace base {
class FilePath;
}

namespace convert_dict {

class AffReader {
 public:
  explicit AffReader(const base::FilePath& path);
  ~AffReader();

  bool Read();

  // Returns whether this file uses indexed affixes, or, on false, whether the
  // rule string will be specified literally in the .dic file. This must be
  // called after Read().
  bool has_indexed_affixes() const { return has_indexed_affixes_; }

  // Returns a string representing the encoding of the dictionary. This will
  // default to ISO-8859-1 if the .aff file does not specify it.
  const char* encoding() const { return encoding_.c_str(); }

  // Converts the given string from the file encoding to UTF-8, returning true
  // on success.
  bool EncodingToUTF8(const std::string& encoded, std::string* utf8) const;

  // Adds a new affix string, returning the index. If it already exists, returns
  // the index of the existing one. This is used to convert .dic files which
  // list the
  // You must not call this until after Read();
  int GetAFIndexForAFString(const std::string& af_string);

  // Getters for the computed data.
  const std::string& comments() const { return intro_comment_; }
  const std::vector<std::string>& affix_rules() const { return affix_rules_; }
  const std::vector< std::pair<std::string, std::string> >&
      replacements() const {
    return replacements_;
  }
  const std::vector<std::string>& other_commands() const {
    return other_commands_;
  }

  // Returns the affix groups ("AF" lines) for this file. The indices into this
  // are 1-based, but we don't use the 0th item, so lookups will have to
  // subtract one to get the index. This is how hunspell stores this data.
  std::vector<std::string> GetAffixGroups() const;

 private:
  // Command-specific handlers. These are given the string following the
  // command. The input rule may be modified arbitrarily by the function.
  int AddAffixGroup(std::string* rule);  // Returns the new affix group ID.
  void AddAffix(std::string* rule);      // SFX/PFX
  void AddReplacement(std::string* rule);
  // void HandleFlag(std::string* rule);

  // Used to handle "other" commands. The "raw" just saves the line as-is.
  // The "encoded" version converts the line to UTF-8 and saves it.
  void HandleRawCommand(const std::string& line);
  void HandleEncodedCommand(const std::string& line);

  FILE* file_;

  // Comments from the beginning of the file. This is everything before the
  // first command. We want to store this since it often contains the copyright
  // information.
  std::string intro_comment_;

  // Encoding of the source words.
  std::string encoding_;

  // Affix rules. These are populated by "AF" commands. The .dic file can refer
  // to these by index. They are indexed by their string value (the list of
  // characters representing rules), and map to the numeric affix IDs.
  //
  // These can also be added using GetAFIndexForAFString.
  std::map<std::string, int> affix_groups_;

  // True when the affixes were specified in the .aff file using indices. The
  // dictionary reader uses this to see how it should treat the stuff after the
  // word on each line.
  bool has_indexed_affixes_;

  // SFX and PFX commands. This is a list of each of those lines in the order
  // they appear in the file. They have been re-encoded.
  std::vector<std::string> affix_rules_;

  // Replacement commands. The first string is a possible input, and the second
  // is the replacment.
  std::vector< std::pair<std::string, std::string> > replacements_;

  // All other commands.
  std::vector<std::string> other_commands_;
};

}  // namespace convert_dict

#endif  // CHROME_TOOLS_CONVERT_DICT_AFF_READER_H__
