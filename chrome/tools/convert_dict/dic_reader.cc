// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/convert_dict/dic_reader.h"

#include <stddef.h>

#include <algorithm>
#include <set>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "chrome/tools/convert_dict/aff_reader.h"
#include "chrome/tools/convert_dict/hunspell_reader.h"

namespace convert_dict {

namespace {

// Maps each unique word to the unique affix group IDs associated with it.
typedef std::map<std::string, std::set<int> > WordSet;

void SplitDicLine(const std::string& line, std::vector<std::string>* output) {
  // We split the line on a slash not preceded by a backslash. A slash at the
  // beginning of the line is not a separator either.
  size_t slash_index = line.size();
  for (size_t i = 0; i < line.size(); i++) {
    if (line[i] == '/' && i > 0 && line[i - 1] != '\\') {
      slash_index = i;
      break;
    }
  }

  output->clear();

  // Everything before the slash index is the first term. We also need to
  // convert all escaped slashes ("\/" sequences) to regular slashes.
  std::string word = line.substr(0, slash_index);
  base::ReplaceSubstringsAfterOffset(&word, 0, "\\/", "/");
  output->push_back(word);

  // Everything (if anything) after the slash is the second.
  if (slash_index < line.size() - 1)
    output->push_back(line.substr(slash_index + 1));
}

// This function reads words from a .dic file, or a .dic_delta file. Note that
// we read 'all' the words in the file, irrespective of the word count given
// in the first non empty line of a .dic file. Also note that, for a .dic_delta
// file, the first line actually does _not_ have the number of words. In order
// to control this, we use the |file_has_word_count_in_the_first_line|
// parameter to tell this method whether the first non empty line in the file
// contains the number of words or not. If it does, skip the first line. If it
// does not, then the first line contains a word.
bool PopulateWordSet(WordSet* word_set, FILE* file, AffReader* aff_reader,
                     const char* file_type, const char* encoding,
                     bool file_has_word_count_in_the_first_line) {
  int line_number = 0;
  while (!feof(file)) {
    std::string line = ReadLine(file);
    line_number++;
    StripComment(&line);
    if (line.empty())
      continue;

    if (file_has_word_count_in_the_first_line) {
      // Skip the first nonempty line, this is the line count. We don't bother
      // with it and just read all the lines.
      file_has_word_count_in_the_first_line = false;
      continue;
    }

    std::vector<std::string> split;
    SplitDicLine(line, &split);
    if (split.empty() || split.size() > 2) {
      printf("Line %d has extra slashes in the %s file\n", line_number,
             file_type);
      return false;
    }

    // The first part is the word, the second (optional) part is the affix. We
    // always use UTF-8 as the encoding to simplify life.
    std::string utf8word;
    std::string encoding_string(encoding);
    if (encoding_string == "UTF-8") {
      utf8word = split[0];
    } else if (!aff_reader->EncodingToUTF8(split[0], &utf8word)) {
      printf("Unable to convert line %d from %s to UTF-8 in the %s file\n",
             line_number, encoding, file_type);
      return false;
    }

    // We always convert the affix to an index. 0 means no affix.
    int affix_index = 0;
    if (split.size() == 2) {
      // Got a rule, which is the stuff after the slash. The line may also have
      // an optional term separated by a tab. This is the morphological
      // description. We don't care about this (it is used in the tests to
      // generate a nice dump), so we remove it.
      size_t split1_tab_offset = split[1].find('\t');
      if (split1_tab_offset != std::string::npos)
        split[1] = split[1].substr(0, split1_tab_offset);

      if (aff_reader->has_indexed_affixes())
        affix_index = atoi(split[1].c_str());
      else
        affix_index = aff_reader->GetAFIndexForAFString(split[1]);
    }

    // Discard the morphological description if it is attached to the first
    // token. (It is attached to the first token if a word doesn't have affix
    // rules.)
    size_t word_tab_offset = utf8word.find('\t');
    if (word_tab_offset != std::string::npos)
      utf8word = utf8word.substr(0, word_tab_offset);

    auto found = word_set->find(utf8word);
    std::set<int> affix_vector;
    affix_vector.insert(affix_index);

    if (found == word_set->end())
      word_set->insert(std::make_pair(utf8word, affix_vector));
    else
      found->second.insert(affix_index);
  }

  return true;
}

}  // namespace

DicReader::DicReader(const base::FilePath& path) {
  file_ = base::OpenFile(path, "r");

  base::FilePath additional_path =
      path.ReplaceExtension(FILE_PATH_LITERAL("dic_delta"));
  additional_words_file_ = base::OpenFile(additional_path, "r");

  if (additional_words_file_)
    printf("Reading %" PRFilePath " ...\n", additional_path.value().c_str());
  else
    printf("%" PRFilePath " not found.\n", additional_path.value().c_str());
}

DicReader::~DicReader() {
  if (file_)
    base::CloseFile(file_);
  if (additional_words_file_)
    base::CloseFile(additional_words_file_);
}

bool DicReader::Read(AffReader* aff_reader) {
  if (!file_)
    return false;

  WordSet word_set;

  // Add words from the dic file to the word set.
  // Note that the first line is the word count in the file.
  if (!PopulateWordSet(&word_set, file_, aff_reader, "dic",
                       aff_reader->encoding(), true))
    return false;

  // Add words from the .dic_delta file to the word set, if it exists.
  // The first line is the first word to add. Word count line is not present.
  // NOTE: These additional words should be encoded as UTF-8.
  if (additional_words_file_ != NULL) {
    PopulateWordSet(&word_set, additional_words_file_, aff_reader, "dic delta",
                    "UTF-8", false);
  }
  // Make sure the words are sorted, they may be unsorted in the input.
  for (auto word = word_set.begin(); word != word_set.end(); ++word) {
    std::vector<int> affixes;
    for (auto aff = word->second.begin(); aff != word->second.end(); ++aff)
      affixes.push_back(*aff);

    // Double check that the affixes are sorted. This isn't strictly necessary
    // but it's nice for the file to have a fixed layout.
    std::sort(affixes.begin(), affixes.end());
    std::reverse(affixes.begin(), affixes.end());
    words_.push_back(std::make_pair(word->first, affixes));
  }

  // Double-check that the words are sorted.
  std::sort(words_.begin(), words_.end());
  return true;
}

}  // namespace convert_dict
