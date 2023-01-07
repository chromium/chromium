// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_KP_ENTITY_RESULT_PARSER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_KP_ENTITY_RESULT_PARSER_H_

#include "base/values.h"
#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

namespace quick_answers {

class KpEntityResultParser : public ResultParser {
 public:
  // ResultParser:
  bool Parse(const base::Value::Dict& result,
             QuickAnswer* quick_answer) override;
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_SEARCH_RESULT_PARSERS_KP_ENTITY_RESULT_PARSER_H_
