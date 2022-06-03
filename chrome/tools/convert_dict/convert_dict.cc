// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tool converts Hunspell .aff/.dic pairs to a combined binary dictionary
// format (.bdic). This format is more compact, and can be more efficiently
// read by the client application.
//
// We do this conversion manually before publishing dictionary files. It is not
// part of any build process.
//
// See PrintHelp() below for usage.

#include <stddef.h>
#include <stdio.h>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/process/memory.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/tools/convert_dict/aff_reader.h"
#include "chrome/tools/convert_dict/dic_reader.h"
#include "third_party/hunspell/google/bdict_reader.h"
#include "third_party/hunspell/google/bdict_writer.h"

namespace {

// Compares the given word list with the serialized trie to make sure they
// are the same.
bool VerifyWords(const convert_dict::DicReader::WordList& org_words,
                 const std::string& serialized) {
  hunspell::BDictReader reader;
  if (!reader.Init(reinterpret_cast<const unsigned char*>(serialized.data()),
                   serialized.size())) {
    printf("BDict is invalid\n");
    return false;
  }
  hunspell::WordIterator iter = reader.GetAllWordIterator();

  int affix_ids[hunspell::BDict::MAX_AFFIXES_PER_WORD];

  static const int buf_size = 128;
  char buf[buf_size];
  for (size_t i = 0; i < org_words.size(); i++) {
    int affix_matches = iter.Advance(buf, buf_size, affix_ids);
    if (affix_matches == 0) {
      printf("Found the end before we expected\n");
      return false;
    }

    if (org_words[i].first != buf) {
      printf("Word doesn't match, word #%s\n", buf);
      return false;
    }

    if (affix_matches != static_cast<int>(org_words[i].second.size())) {
      printf("Different number of affix indices, word #%s\n", buf);
      return false;
    }

    // Check the individual affix indices.
    for (size_t affix_index = 0; affix_index < org_words[i].second.size();
         affix_index++) {
      if (affix_ids[affix_index] != org_words[i].second[affix_index]) {
        printf("Index doesn't match, word #%s\n", buf);
        return false;
      }
    }
  }

  return true;
}

int PrintHelp() {
  printf("Usage: convert_dict <dicfile base name>\n\n");
  printf("Example:\n");
  printf("  convert_dict en-US\nwill read en-US.dic, en-US.dic_delta, and "
         "en-US.aff from the current directory and generate en-US.bdic\n\n");
  return 1;
}

}  // namespace

#if defined(OS_WIN)
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
  base::EnableTerminationOnHeapCorruption();
  if (argc != 2)
    return PrintHelp();

  base::AtExitManager exit_manager;
  base::i18n::InitializeICU();

  base::FilePath file_base = base::FilePath(argv[1]);

  base::FilePath aff_path =
      file_base.ReplaceExtension(FILE_PATH_LITERAL(".aff"));
  printf("Reading %" PRFilePath " ...\n", aff_path.value().c_str());
  convert_dict::AffReader aff_reader(aff_path);
  if (!aff_reader.Read()) {
    printf("Unable to read the aff file.\n");
    return 1;
  }

  base::FilePath dic_path =
      file_base.ReplaceExtension(FILE_PATH_LITERAL(".dic"));
  printf("Reading %" PRFilePath " ...\n", dic_path.value().c_str());
  // DicReader will also read the .dic_delta file.
  convert_dict::DicReader dic_reader(dic_path);
  if (!dic_reader.Read(&aff_reader)) {
    printf("Unable to read the dic file.\n");
    return 1;
  }

  hunspell::BDictWriter writer;
  writer.SetComment(aff_reader.comments());
  writer.SetAffixRules(aff_reader.affix_rules());
  writer.SetAffixGroups(aff_reader.GetAffixGroups());
  writer.SetReplacements(aff_reader.replacements());
  writer.SetOtherCommands(aff_reader.other_commands());
  writer.SetWords(dic_reader.words());

  printf("Serializing...\n");
  std::string serialized = writer.GetBDict();

  printf("Verifying...\n");
  if (!VerifyWords(dic_reader.words(), serialized)) {
    printf("ERROR converting, the dictionary does not check out OK.");
    return 1;
  }

  base::FilePath out_path =
      file_base.ReplaceExtension(FILE_PATH_LITERAL(".bdic"));
  printf("Writing %" PRFilePath " ...\n", out_path.value().c_str());
  FILE* out_file = base::OpenFile(out_path, "wb");
  if (!out_file) {
    printf("ERROR writing file\n");
    return 1;
  }
  size_t written = fwrite(&serialized[0], 1, serialized.size(), out_file);
  CHECK(written == serialized.size());
  base::CloseFile(out_file);

  return 0;
}
