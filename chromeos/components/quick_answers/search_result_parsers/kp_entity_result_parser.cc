// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/kp_entity_result_parser.h"

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"

namespace quick_answers {
namespace {

using base::Value;

constexpr char kKnowledgeEntityPath[] = "knowledgePanelEntityResult.entity";
constexpr char kScorePath[] =
    "ratingsAndReviews.google.aggregateRating.averageScore";
constexpr char kRatingCountPath[] =
    "ratingsAndReviews.google.aggregateRating.aggregatedCount";
constexpr char kKnownForReasonPath[] = "localizedKnownForReason";
constexpr char kAverageScoreTemplate[] = "%.1f";

}  // namespace

// Extract |quick_answer| from knowledge panel entity result.
bool KpEntityResultParser::Parse(const Value::Dict& result,
                                 QuickAnswer* quick_answer) {
  const auto* entity = result.FindDictByDottedPath(kKnowledgeEntityPath);
  if (!entity) {
    LOG(ERROR) << "Can't find the knowledge panel entity.";
    return false;
  }

  const auto average_score = entity->FindDoubleByDottedPath(kScorePath);
  const auto* aggregated_count =
      entity->FindStringByDottedPath(kRatingCountPath);

  if (average_score.has_value() && aggregated_count) {
    const auto& answer = BuildKpEntityTitleText(
        base::StringPrintf(kAverageScoreTemplate, average_score.value()),
        aggregated_count->c_str());
    quick_answer->first_answer_row.push_back(
        std::make_unique<QuickAnswerResultText>(answer));
  } else {
    const std::string* localized_known_for_reason =
        entity->FindStringByDottedPath(kKnownForReasonPath);
    if (!localized_known_for_reason) {
      LOG(ERROR) << "Can't find the localized known for reason field.";
      return false;
    }

    quick_answer->first_answer_row.push_back(
        std::make_unique<QuickAnswerResultText>(*localized_known_for_reason));
  }

  return true;
}

}  // namespace quick_answers
