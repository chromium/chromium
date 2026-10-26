// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_FIELD_PARSER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_FIELD_PARSER_H_

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/memory/stack_allocated.h"
#include "base/types/expected.h"

namespace password_manager {

// CSVFieldParser is created for a row (line) of comma-separated-values and
// iteratively returns individual fields.
class CSVFieldParser {
  STACK_ALLOCATED();

 public:
  struct SyntaxError {};

  // Maximum number of fields accepted.
  static constexpr size_t kMaxFields = 100;

  explicit CSVFieldParser(std::string_view row LIFETIME_BOUND);
  ~CSVFieldParser();

  CSVFieldParser(const CSVFieldParser&) = delete;
  CSVFieldParser& operator=(const CSVFieldParser&) = delete;

  // Advances the parser over the next comma-separated field and returns its
  // contents (comma separator excluded, enclosing quotation marks excluded, if
  // present), or std::nullopt if there are no more fields.
  std::optional<base::expected<std::string_view, SyntaxError>> NextField()
      LIFETIME_BOUND;

 private:
  std::optional<std::string_view> rest_;

  // The number of past invocations of NextField().
  size_t fields_returned_ = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_CSV_FIELD_PARSER_H_
