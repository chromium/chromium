// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/convert_dict/hunspell_reader.h"

#include <stddef.h>
#include <stdio.h>

#include "base/strings/string_util.h"

namespace convert_dict {

// Shortcut for trimming whitespace from both ends of the line.
void TrimLine(std::string* line) {
  if (line->size() > 3 &&
      static_cast<unsigned char>((*line)[0]) == 0xef &&
      static_cast<unsigned char>((*line)[1]) == 0xbb &&
      static_cast<unsigned char>((*line)[2]) == 0xbf)
    *line = line->substr(3);

  // Treat this text as an ASCII text and trim whitespace characters as
  // hunspell does. The returned text is to be converted into UTF-8 text with
  // the encoding defined in an affix file.
  base::TrimWhitespaceASCII(*line, base::TRIM_ALL, line);
}

std::string ReadLine(FILE* file) {
  // Like fgets(), read up to and including the next newline (or until EOF),
  // but without any limit on the line length. The newline is kept so that
  // TrimLine() sees the same input as before (e.g. for BOM-only lines); it is
  // then trimmed along with the other whitespace.
  //
  // To match the previous fgets() + std::string(const char*) behavior, bytes
  // from an embedded NUL up to the end of the line are dropped, and an empty
  // string is returned on a read error.
  std::string line;
  bool saw_nul = false;
  for (int c = getc(file); c != EOF; c = getc(file)) {
    if (c == '\0') {
      saw_nul = true;
    }
    if (!saw_nul) {
      line.push_back(static_cast<char>(c));
    }
    if (c == '\n') {
      break;
    }
  }
  if (ferror(file)) {
    return std::string();
  }
  TrimLine(&line);
  return line;
}

void StripComment(std::string* line) {
  for (size_t i = 0; i < line->size(); i++) {
    if ((*line)[i] == '#') {
      line->resize(i);
      TrimLine(line);
      return;
    }
  }
}

}  // namespace convert_dict
