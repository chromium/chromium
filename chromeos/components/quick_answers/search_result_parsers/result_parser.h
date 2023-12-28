// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_RESULT_PARSER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_RESULT_PARSER_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace quick_answers {

// Parser interface.
class ResultParser {
 public:
  virtual ~ResultParser() = default;

  // Helper parser function to get the first element in a value list, which is
  // expected to be a dictionary.
  static const base::Value::Dict* GetFirstDictElementFromList(
      const base::Value::Dict& dict,
      const std::string& path);

  // Helper parser function to remove known HTML tags from a std::string.
  static std::string RemoveKnownHtmlTags(const std::string& input);

  // Parse the result into `quick_answer`. All `ResultParser`s must support this
  // for now for backward compatibility reason. `Parse` method would be deleted
  // after we migrate interfaces of all parsers.
  virtual bool Parse(const base::Value::Dict& result,
                     QuickAnswer* quick_answer) = 0;

  // Interfaces for supporting Rich Answers.
  virtual std::unique_ptr<StructuredResult> ParseInStructuredResult(
      const base::Value::Dict& result);

  // `quick_answer` can be modified even if `PopulateQuickAnswer` returns false,
  // i.e. do not assume that `quick_answer` is un-modified if this method
  // returns false.
  virtual bool PopulateQuickAnswer(const StructuredResult& structured_result,
                                   QuickAnswer* quick_answer);

  // Returns true if this parser supports the new interfaces. Note that all
  // parsers must support old interfaces even if it supports new interfaces.
  virtual bool SupportsNewInterface() const;
};

// A factory class for creating ResultParser based on the |one_namespace_type|.
class ResultParserFactory {
 public:
  // Creates ResultParser based on the |one_namespace_type|.
  static std::unique_ptr<ResultParser> Create(int one_namespace_type);

  virtual ~ResultParserFactory() = default;
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_RESULT_PARSER_H_
