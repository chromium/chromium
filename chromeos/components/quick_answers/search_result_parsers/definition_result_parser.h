// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_DEFINITION_RESULT_PARSER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_DEFINITION_RESULT_PARSER_H_

#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

namespace base {
class GURL;
class Value;
}  // namespace base

namespace quick_answers {

class DefinitionResultParser : public ResultParser {
 public:
  // ResultParser:
  bool Parse(const base::Value* result, QuickAnswer* quick_answer) override;

 private:
  const base::Value* ExtractFirstSenseFamily(
      const base::Value* definition_entry);
  const base::Value* ExtractFirstPhonetics(const base::Value* definition_entry);
  const std::string* ExtractDefinition(const base::Value* definition_entry);
  const std::string* ExtractPhoneticsText(const base::Value* definition_entry);
  void ExtractPhoneticsInfo(PhoneticsInfo* phonetics_info,
                            const base::Value* definition_entry);
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_DEFINITION_RESULT_PARSER_H_
