// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains matchers useful for testing with parsed PPD
// metadata.

#ifndef CHROMEOS_PRINTING_PPD_METADATA_MATCHERS_H_
#define CHROMEOS_PRINTING_PPD_METADATA_MATCHERS_H_

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chromeos/printing/ppd_metadata_parser.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace chromeos {

using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Optional;
using ::testing::StrEq;

MATCHER_P(RestrictionsWithMinMilestone,
          integral_min_milestone,
          "is a Restrictions with min_milestone ``" +
              base::NumberToString(integral_min_milestone) + "''") {
  return ExplainMatchResult(
      Field(&Restrictions::min_milestone,
            Optional(base::Version(base::NumberToString(
                static_cast<double>(integral_min_milestone))))),
      arg, result_listener);
}

MATCHER_P(RestrictionsWithMaxMilestone,
          integral_max_milestone,
          "is a Restrictions with max_milestone ``" +
              base::NumberToString(integral_max_milestone) + "''") {
  return ExplainMatchResult(
      Field(&Restrictions::max_milestone,
            Optional(base::Version(base::NumberToString(
                static_cast<double>(integral_max_milestone))))),
      arg, result_listener);
}

MATCHER_P2(RestrictionsWithMinAndMaxMilestones,
           integral_min_milestone,
           integral_max_milestone,
           "is a Restrictions with min_milestone ``" +
               base::NumberToString(integral_min_milestone) +
               "'' "
               "and max_milestone ``" +
               base::NumberToString(integral_max_milestone) + "''") {
  return ExplainMatchResult(
             RestrictionsWithMinMilestone(integral_min_milestone), arg,
             result_listener) &&
         ExplainMatchResult(
             RestrictionsWithMaxMilestone(integral_max_milestone), arg,
             result_listener);
}

MATCHER(UnboundedRestrictions,
        "is a Restrictions with neither min nor max milestones") {
  return ExplainMatchResult(
             Field(&Restrictions::min_milestone, Eq(std::nullopt)), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&Restrictions::max_milestone, Eq(std::nullopt)), arg,
             result_listener);
}

// Matches a ReverseIndexLeaf struct against its |manufacturer| and
// |model| members.
MATCHER_P2(ReverseIndexLeafLike,
           manufacturer,
           model,
           "is a ReverseIndexLeaf with manufacturer ``" +
               std::string(manufacturer) + "'' and model ``" +
               std::string(model) + "''") {
  return ExplainMatchResult(
             Field(&ReverseIndexLeaf::manufacturer, StrEq(manufacturer)), arg,
             result_listener) &&
         ExplainMatchResult(Field(&ReverseIndexLeaf::model, StrEq(model)), arg,
                            result_listener);
}

// Matches a ParsedIndexLeaf struct against its
// |ppd_basename| member.
MATCHER_P(ParsedIndexLeafWithPpdBasename,
          ppd_basename,
          "is a ParsedIndexLeaf with ppd_basename ``" +
              std::string(ppd_basename) + "''") {
  return ExplainMatchResult(
      Field(&ParsedIndexLeaf::ppd_basename, StrEq(ppd_basename)), arg,
      result_listener);
}

// Matches a key-value pair in a ParsedIndex against its constituent
// members. |parsed_index_leaf_matcher| is matched against the |values|
// member of a ParsedIndexValues struct.
MATCHER_P2(ParsedIndexEntryLike,
           emm,
           parsed_index_leaf_matcher,
           "is a ParsedIndex entry with effective-make-and-model string ``" +
               std::string(emm) + "''") {
  return ExplainMatchResult(Pair(StrEq(emm), Field(&ParsedIndexValues::values,
                                                   parsed_index_leaf_matcher)),
                            arg, result_listener);
}

// Matches a ParsedPrinter struct against its
// |user_visible_printer_name| and |effective_make_and_model| members.
MATCHER_P2(ParsedPrinterLike,
           name,
           emm,
           "is a ParsedPrinter with user_visible_printer_name``" +
               std::string(name) + "'' and effective_make_and_model ``" +
               std::string(emm) + "''") {
  return ExplainMatchResult(
             Field(&ParsedPrinter::user_visible_printer_name, StrEq(name)), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&ParsedPrinter::effective_make_and_model, StrEq(emm)), arg,
             result_listener);
}

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_METADATA_MATCHERS_H_
