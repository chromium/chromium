// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

// From crbug.com/774858
struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // Used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment icu_env;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // This is an arbitrary size, and arguably even small for a JSON input,
  // but we have to cut it off somewhere.
  if (size > 4096)
    return 0;
  std::unique_ptr<std::string> response_body =
      std::make_unique<std::string>(reinterpret_cast<const char*>(data), size);
  std::unique_ptr<base::Value> value(
      SearchSuggestionParser::DeserializeJsonData(
          SearchSuggestionParser::ExtractJsonData(nullptr,
                                                  std::move(response_body))));
  if (value) {
    AutocompleteInput input;
    {
      // Set-up the input so downstream won't reject it.
      if (value->is_list()) {
        base::Value::ConstListView root_list = value->GetList();
        if (!root_list.empty() && root_list[0].is_string()) {
          std::string query = root_list[0].GetString();
          input = AutocompleteInput(base::UTF8ToUTF16(query),
                                    metrics::OmniboxEventProto::OTHER,
                                    TestSchemeClassifier());
        }
      }
    }
    // This is primarily only used to decide where to store the results,
    // and to record in the SuggestResult.
    const bool is_keyword = false;
    SearchSuggestionParser::Results results;
    // Copied from BaseSearchProvider::ParseSuggestResults()
    SearchSuggestionParser::ParseSuggestResults(
        *value, input, TestSchemeClassifier(), -1, is_keyword, &results);
  }
  return 0;
}
