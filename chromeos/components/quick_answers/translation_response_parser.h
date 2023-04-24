// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_TRANSLATION_RESPONSE_PARSER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_TRANSLATION_RESPONSE_PARSER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace quick_answers {

// Parser for extracting quick answer result out of the cloud translation
// response.
class TranslationResponseParser {
 public:
  // Callback used when parsing for `translation_result` is complete. Note that
  // `translation_result` may be `nullptr`.
  using TranslationResponseParserCallback = base::OnceCallback<void(
      std::unique_ptr<TranslationResult> translation_result)>;

  explicit TranslationResponseParser(
      TranslationResponseParserCallback complete_callback);
  ~TranslationResponseParser();

  TranslationResponseParser(const TranslationResponseParser&) = delete;
  TranslationResponseParser& operator=(const TranslationResponseParser&) =
      delete;

  // Starts processing the search response.
  void ProcessResponse(std::unique_ptr<std::string> response_body);

 private:
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  TranslationResponseParserCallback complete_callback_;

  base::WeakPtrFactory<TranslationResponseParser> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_TRANSLATION_RESPONSE_PARSER_H_
