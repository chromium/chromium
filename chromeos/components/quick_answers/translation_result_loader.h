// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_TRANSLATION_RESULT_LOADER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_TRANSLATION_RESULT_LOADER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/result_loader.h"
#include "chromeos/components/quick_answers/translation_response_parser.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace quick_answers {

class TranslationResultLoader : public ResultLoader {
 public:
  TranslationResultLoader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ResultLoaderDelegate* delegate);

  TranslationResultLoader(const TranslationResultLoader&) = delete;
  TranslationResultLoader& operator=(const TranslationResultLoader&) = delete;

  ~TranslationResultLoader() override;

  // ResultLoader:
  void BuildRequest(const PreprocessedOutput& preprocessed_output,
                    BuildRequestCallback callback) const override;
  void ProcessResponse(const PreprocessedOutput& preprocessed_output,
                       std::unique_ptr<std::string> response_body,
                       ResponseParserCallback complete_callback) override;

 private:
  void ProcessParsedResponse(
      IntentInfo intent_info,
      ResponseParserCallback complete_callback,
      std::unique_ptr<TranslationResult> translation_result);
  std::unique_ptr<TranslationResponseParser> translation_response_parser_;

  base::WeakPtrFactory<TranslationResultLoader> weak_ptr_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_TRANSLATION_RESULT_LOADER_H_
