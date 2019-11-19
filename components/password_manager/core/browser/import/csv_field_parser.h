// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_FIELD_PARSER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_FIELD_PARSER_H_

#include <stddef.h>

#include "base/strings/string_piece.h"

namespace password_manager {

// CSVFieldParser is created for a row (line) of comma-separated-values and
// iteratively returns individual fields.
class CSVFieldParser {
 public:
  // Maximum number of fields accepted.
  constexpr static size_t kMaxFields = 100;

  explicit CSVFieldParser(base::StringPiece row);
  ~CSVFieldParser();

  CSVFieldParser(const CSVFieldParser&) = delete;
  CSVFieldParser& operator=(const CSVFieldParser&) = delete;

  // Advances the parser over the next comma-separated field and writes its
  // contents into |field_contents| (comma separator excluded, enclosing
  // quotation marks excluded, if present). Returns true if there were no
  // errors. The input must not be empty (check with HasMoreFields() before
  // calling).
  bool NextField(base::StringPiece* field_contents);

  bool HasMoreFields() const {
    return state_ != State::kError && position_ <= row_.size();
  }

 private:
  enum class State {
    // The state just before a new field begins.
    kInit,
    // The state after parsing a syntax error.
    kError,
    // When inside a non-escaped block.
    kPlain,
    // When inside a quotation-mark-escaped block.
    kQuoted,
    // When after reading a block starting and ending with quotation marks. For
    // the following input, the state would be visited after reading characters
    // 4 and 7:
    // a,"b""c",d
    // 0123456789
    kAfter,
  };

  // Returns the next character to be read and updates |position_|.
  char ConsumeChar();

  // Updates |state_| based on the next character to be read, according to this
  // diagram (made with help of asciiflow.com):
  //
  //   ,
  //  +--+  +--------------------------+
  //  |  |  |                          |
  // +V--+--V+all but " or , +--------+|
  // |       +--------------->        ||
  // | kInit |               | kPlain ||
  // |       <---------------+        ||
  // ++------+      ,        +^------++|
  //  |                       |      | |
  // "|                       +------+ |
  //  |                    all but ,   |,
  //  |                                |
  //  |                                |
  //  |   +---------+    "     +-------++
  //  |   |         +---------->        |
  //  +---> kQuoted |          | kAfter |
  //      |         <----------+        |
  //      +---------+    "     +-----+--+
  //                                |
  //      +--------+                |
  //      |        |                |
  //      | kError <----------------+
  //      |        |   all but " or ,
  //      +--------+
  //
  // The state kError has no outgoing transitions and so UpdateState should not
  // be called when this state has been entered.
  void UpdateState();

  // State of the parser.
  State state_ = State::kInit;
  // The input.
  const base::StringPiece row_;
  // If |position_| is >=0 and < |row_.size()|, then it points at the character
  // to be read next from |row_|. If it is equal to |row_.size()|, then it means
  // a fake trailing "," will be read next. If it is |row_.size() + 1|, then
  // reading is done.
  size_t position_ = 0;

  // The number of successful past invocations of NextField().
  size_t fields_returned_ = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_FIELD_PARSER_H_
