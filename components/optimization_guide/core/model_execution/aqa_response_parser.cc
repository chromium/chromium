// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/aqa_response_parser.h"

#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// Expected model output when the question is unanswerable.
auto UnanswerableRegex() {
  static const LazyRE2 re = {"((?i)Unanswerable\\.?)((.|\n)*)"};
  return re.get();
}

// Regex for a passage ID, capturing ID numbers.
// The start ID in the answer does not contain the `ID` token.
// The IDs should be comma-delimited, but there could be arbitrary whitespaces.
auto StartIdRegex() {
  static const LazyRE2 re = {"\\s*([0-9,\\s]*[0-9])"};
  return re.get();
}

// Regex for capture potentially recursive occurrence of IDs in the middle of
// the answer.
auto IdRegex() {
  static const LazyRE2 re = {"ID:\\s*([0-9,\\s]*[0-9])"};
  return re.get();
}
// Suffix that comes after a citation of passage IDs.
constexpr std::string_view kCitationSuffix = " has the answer";
constexpr std::string_view kAnswerPrefix = "The answer is ";

// Parse the given string as an AQA model response, and return the response
// message wrapped in an optimization_guide::proto::Any object. If parsing
// fails, return the corresponding failure enum.
optimization_guide::AqaResponseParser::Result ParseAqaResponse(
    const std::string& redacted_output) {
  optimization_guide::proto::HistoryAnswerResponse history_answer_response;
  optimization_guide::proto::Any any;
  any.set_type_url("type.googleapis.com/" +
                   history_answer_response.GetTypeName());

  std::string unanswerable_capture;
  std::string remaining_capture;
  // Check if the output matches the unanswerable string.
  if (re2::RE2::FullMatch(redacted_output, *UnanswerableRegex(),
                          &unanswerable_capture, &remaining_capture)) {
    history_answer_response.set_is_unanswerable(true);
    history_answer_response.SerializeToString(any.mutable_value());
    return any;
  }

  // Capture passage IDs.
  std::string raw_prediction;
  base::TrimWhitespaceASCII(redacted_output, base::TRIM_ALL, &raw_prediction);
  std::string_view remaining_prediction_view(raw_prediction);
  std::string id_string;
  if (!re2::RE2::Consume(&remaining_prediction_view, *StartIdRegex(),
                         &id_string)) {
    // Failed to match any ID; the prediction is not parsable.
    return base::unexpected(optimization_guide::ResponseParsingError::kFailed);
  }

  re2::RE2::FindAndConsume(&remaining_prediction_view, kCitationSuffix);
  // Optionally remove punctuation and whitespace after the suffix.
  re2::RE2::Consume(&remaining_prediction_view, "\\.?\\s*");
  re2::RE2::FindAndConsume(&remaining_prediction_view, kAnswerPrefix);

  std::string answer_string = std::string(remaining_prediction_view);
  if (re2::RE2::PartialMatch(answer_string, *IdRegex())) {
    // Parsed answer still has a passage ID, indicating a recursive model
    // response.
    return base::unexpected(optimization_guide::ResponseParsingError::kFailed);
  }
  if (re2::RE2::FullMatch(remaining_prediction_view, *UnanswerableRegex(),
                          &unanswerable_capture, &remaining_capture)) {
    // This handles the special case response of the form "ID:xxxx has the
    // answer. The answer is Unanswerable."
    history_answer_response.set_is_unanswerable(true);
    history_answer_response.SerializeToString(any.mutable_value());
    return any;
  }

  std::vector<std::string> split_id_strings = base::SplitString(
      id_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  auto* answer = history_answer_response.mutable_answer();
  answer->set_text(answer_string);
  for (std::string& id : split_id_strings) {
    answer->add_citations()->set_passage_id(id);
  }
  history_answer_response.SerializeToString(any.mutable_value());
  return any;
}

}  // namespace

namespace optimization_guide {

AqaResponseParser::AqaResponseParser(
    const proto::OnDeviceModelExecutionOutputConfig& config)
    : config_(config) {}
AqaResponseParser::~AqaResponseParser() = default;

void AqaResponseParser::ParseAsync(const std::string& redacted_output,
                                   ResultCallback result_callback) const {
  std::move(result_callback).Run(ParseAqaResponse(redacted_output));
}

bool AqaResponseParser::SuppressParsingIncompleteResponse() const {
  // AQA can only parse complete responses.
  return true;
}

AqaResponseParserFactory::AqaResponseParserFactory() = default;
AqaResponseParserFactory::~AqaResponseParserFactory() = default;

std::unique_ptr<ResponseParser> AqaResponseParserFactory::CreateParser(
    const proto::OnDeviceModelExecutionOutputConfig& config) {
  // Can only parse to HistoryAnswerResponse.
  if (config.proto_type() != "optimization_guide.proto.HistoryAnswerResponse") {
    return nullptr;
  }
  return std::make_unique<AqaResponseParser>(config);
}

}  // namespace optimization_guide
