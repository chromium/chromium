// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_CONFIGURATION_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_CONFIGURATION_PARSER_H_

#include <string>

#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/version.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

namespace field_type_parsing {

// Tries to extract the configuration version from the JSON base::Value tree.
// This removes the key if found, so that validation is easier later on.
// If not found, default to version 0.
base::Version ExtractVersionFromJsonObject(base::Value& root);

// Transforms the parsed JSON base::Value tree into the map used in
// |PatternProvider|. Requires the version key to already be extracted.
// The root is expected to be a dictionary with keys corresponding to
// strings representing |ServerFieldType|. Then there should be
// second level dictionaries with keys describing the language. These
// should point to a list of objects representing |MatchingPattern|.
//  {
//    "FIELD_NAME": {
//      "language":[
//        {MatchingPatternFields}
//      ]
//    }
//  }
//  An example can be found in the relative resources folder.
absl::optional<PatternProvider::Map> GetConfigurationFromJsonObject(
    const base::Value& root);

// Tries to get and parse the default configuration in the resource bundle
// into a valid map used in |PatternProvider| and swap it in for further use.
// The callback is used as a signal for testing.
void PopulateFromResourceBundle(
    base::OnceClosure done_callback = base::DoNothing());

// Tries to parse the given JSON string into a valid map used in the
// |PatternProvider| and swap it in for further use.
void PopulateFromJsonString(std::string json_string);

// Synchronous getter used to set up a test fixture.
absl::optional<PatternProvider::Map>
GetPatternsFromResourceBundleSynchronously();

}  // namespace field_type_parsing

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_CONFIGURATION_PARSER_H_
