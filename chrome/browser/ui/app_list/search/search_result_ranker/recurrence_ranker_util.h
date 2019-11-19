// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace app_list {

// Returns a new, configured instance of the predictor defined in |config|.
std::unique_ptr<RecurrencePredictor> MakePredictor(
    const RecurrencePredictorConfigProto& config,
    const std::string& model_identifier);

// A utility class for converting a JSON configuration for a RecurrenceRanker
// into a RecurrenceRankerConfigProto that can be used to construct the ranker.
// JSON parsing is performed safely on another thread.
//
// The JSON configuration format is similar, but not identical, to the
// RecurrenceRankerConfigProto schema. The unit tests are the best
// specification of the format.
class JsonConfigConverter {
 public:
  using OnConfigLoadedCallback =
      base::OnceCallback<void(base::Optional<RecurrenceRankerConfigProto>)>;

  // Creates a JsonConfigConverter and starts a conversion of |json_string|.
  // |model_identifier| is used for metrics reporting in the same way as
  // RecurrenceRanker's |model_identifier|.
  //
  // The provided |callback| will be called with the resulting proto if the
  // conversion succeeded, or base::nullopt if the parsing or conversion failed.
  // If the returned JsonConfigConverter instance is destroyed before parsing is
  // complete, |callback| will never be called.
  //
  // |callback| should destroy the returned JsonConfigConverter instance.
  static std::unique_ptr<JsonConfigConverter> Convert(
      const std::string& json_string,
      const std::string& model_identifier,
      OnConfigLoadedCallback callback);

  ~JsonConfigConverter();

 private:
  JsonConfigConverter();

  // Performs a conversion.
  void Start(const std::string& json_string,
             const std::string& model_identifier,
             OnConfigLoadedCallback callback);

  // Callback for parser.
  void OnJsonParsed(OnConfigLoadedCallback callback,
                    const std::string& model_identifier,
                    data_decoder::DataDecoder::ValueOrError result);

  std::string model_identifier_;
  base::WeakPtrFactory<JsonConfigConverter> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_
