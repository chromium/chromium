// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/csv_writer.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

namespace {

// Encapsulates formatting and outputting values to a CSV file row by row. Takes
// care of escaping and adding the appropriate separators.
class CSVFormatter {
 public:
  explicit CSVFormatter(std::string* output)
      : output_(output), at_beginning_of_line_(true) {}

  // Escapes the |raw_value| if needed, adds a field separator unless we are at
  // the beginning of a line, and appends the escaped value to |output_|.
  void AppendValue(const std::string& raw_value);

  // Appends the platform-specific EOL terminator to |output_|.
  void EndLine();

 private:
  raw_ptr<std::string> output_;
  bool at_beginning_of_line_;
};

void CSVFormatter::AppendValue(const std::string& raw_value) {
  // Append the field separator unless this is the first field on the line.
  if (!at_beginning_of_line_) {
    output_->push_back(',');
  }
  at_beginning_of_line_ = false;

  // Fields containing line breaks (CRLF), double quotes, and commas should be
  // enclosed in double-quotes. If double-quotes are used to enclose fields,
  // then double-quotes appearing inside a field must be escaped by preceding
  // them with another double quote.
  if (raw_value.find_first_of("\r\n\",") != std::string::npos) {
    output_->push_back('\"');
    output_->append(raw_value);
    base::ReplaceSubstringsAfterOffset(
        output_, output_->size() - raw_value.size(), "\"", "\"\"");
    output_->push_back('\"');
  } else {
    output_->append(raw_value);
  }
}

void CSVFormatter::EndLine() {
#if BUILDFLAG(IS_WIN)
  const char kLineEnding[] = "\r\n";
#else
  const char kLineEnding[] = "\n";
#endif
  output_->append(kLineEnding);
  at_beginning_of_line_ = true;
}

}  // namespace

namespace password_manager {

void WriteCSV(const std::vector<std::string>& column_names,
              const std::vector<std::map<std::string, std::string>>& records,
              std::string* csv) {
  DCHECK(csv);
  csv->clear();

  // Append header row.
  CSVFormatter formatter(csv);
  for (const auto& column_name : column_names) {
    formatter.AppendValue(column_name);
  }
  formatter.EndLine();

  // Append every other data record row.
  for (const auto& row : records) {
    for (const auto& column_name : column_names) {
      auto it_field = row.find(column_name);
      formatter.AppendValue(it_field != row.end() ? it_field->second
                                                  : std::string());
    }
    formatter.EndLine();
  }
}

}  // namespace password_manager
