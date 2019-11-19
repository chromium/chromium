// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_LINE_READER_H_
#define CHROMEOS_PRINTING_PPD_LINE_READER_H_

#include <memory>
#include <string>

#include "chromeos/chromeos_export.h"

namespace chromeos {

// Class supporting line-oriented input from unencoded and gzip-encoded PPDs.
// Decompression, when required, is streamed to avoid excessive memory usage due
// to malicious gzip contents.
class CHROMEOS_EXPORT PpdLineReader {
 public:
  // Create a new PpdLineReader using ppd contents in |contents|.  The max
  // allowed line length in the ppd is also parameterized.
  //
  // |contents| may or may not be gzip-compressed, and must remain valid and
  // unchanged while the Created PpdReader exists.
  static std::unique_ptr<PpdLineReader> Create(const std::string& contents,
                                               size_t max_line_length);

  // Checks to see whether the file contents in |contents| contains the magic
  // number which is found at the beginning of every PPD file. To verify this,
  // a line reader is created which simply attempts to read the first line and
  // checks whether it contains the magic number.
  static bool ContainsMagicNumber(const std::string& contents,
                                  size_t max_line_length);

  virtual ~PpdLineReader() = default;

  // Get the contents of the next non-empty line from the ppd into |line|.
  // Returns true on success, false if there was nothing left to read or an
  // error occurred.  Lines longer than max_line_length are skipped.  To
  // distinguish between end of input and error, use Error().
  virtual bool NextLine(std::string* line) = 0;

  // Return true if we encountered an error while reading.
  virtual bool Error() const = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_LINE_READER_H_
