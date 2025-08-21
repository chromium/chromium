// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_SERVER_PREDICTION_OVERRIDES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_SERVER_PREDICTION_OVERRIDES_H_

#include <deque>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

using ServerPredictionOverrideKey = std::pair<FormSignature, FieldSignature>;
using ServerPredictionOverrides = base::flat_map<
    ServerPredictionOverrideKey,
    std::deque<AutofillQueryResponse::FormSuggestion::FieldSuggestion>>;

enum class OverrideFormat {
  // Overrides are strings of the format:
  //   override1-override2-override3-...
  // where each override has the following structure:
  //   formsignature_fieldsignature_typeprediction1_typeprediction2_...
  //
  // Here formsignature is the decimal representation of a `FormSignature`,
  // fieldsignature is a decimal representation of a `FieldSignature` and
  // typeprediction is the decimal representation of a FieldType (see
  // components/autofill/core/browser/field_types.h). Any number of type
  // predictions is permitted, including zero. If there are zero type
  // predictions, i.e., the override is formsignature_fieldsignature, then the
  // override acts as a "pass through", i.e. it defaults to server / local
  // predictions (more details in the examples).
  //
  // Example 1: Overrides for a single field:
  // Specification:
  //   10011880710453506489_1654523497_3
  // Effect:
  //   The prediction for the field with signature 1654523497 in the form
  //   with signature 10011880710453506489 is overridden to 3, i.e. NAME_FIRST.
  //
  // Example 2: Overrides for multiple fields:
  // Specification:
  //   10011880710453506489_1654523497_90-10011880710345406489_2001230230_9_100
  // Effect:
  //   The prediction for the field with signature 1654523497 in the form
  //   with signature 10011880710453506489 is overridden to 90, i.e.
  //   NOT_NEW_PASSWORD.
  //   The prediction for the field with signature 2001230230 in the form with
  //   signature 10011880710345406489 is overridden to two predictions, 9
  //   (EMAIL_ADDRESS) and 100 (SINGLE_USERNAME).
  //
  // The more complex cases occur when there are multiple fields with the same
  // form and field signature. In this case, the server can send multiple
  // `FieldSuggestion`s per (form signature, field signature) pair and they are
  // matched to the fields in the order in which they appear in the form. If
  // there are more fields (with this signature pair) in the form than there are
  // server predictions, then all additional fields will receive the last server
  // prediction. Manual overrides follow the same behavior.
  //
  // Example 3: Signature collision
  // Specification:
  //   10011880710453506489_1654523497_3-10011880710453506489_1654523497_5
  // Effect:
  //   The first field with field signature 1654523497 in the form with
  //   signature
  //   10011880710453506489 has type prediction 3 (NAME_FIRST), the second field
  //   with the same signatures has type prediction 5 (NAME_LAST), and if there
  //   are any further fields with the same signatures, they also receive type
  //   prediction 5.
  //
  // Finally, when there are signature collisions, it may be desirable to only
  // override some of the predictions. This can be achieved by using "pass
  // through"s, i.e. omitting a field type prediction.
  //
  // Example 4: Signature collision and pass through for initial field
  // Specification:
  //   10011880710453506489_1654523497-10011880710453506489_1654523497_5
  // Effect:
  //   The first field with field signature 1654523497 in the form with
  //   signature
  //   10011880710453506489 has no manually overridden type specification - it
  //   will be whatever the server or the local heuristics specify. The second
  //   field with the same signatures has type prediction 5 (NAME_LAST), and if
  //   there are any further fields with the same signatures, they also receive
  //   type prediction 5.
  //
  // Example 5: Signature collision and pass through for last field
  // Specification:
  //   10011880710453506489_1654523497_3-10011880710453506489_1654523497
  // Effect:
  //   The first field with field signature 1654523497 in the form with
  //   signature
  //   10011880710453506489 has its type specification manually overridden to 3.
  //   The second field and all other fields that follow will have whatever type
  //   the server (or the local heuristics) prescribe.
  //
  // Example 6: Overriding form signatures and alternative form signatures
  // Specification:
  //     10011880710453506489_1654523497_3-10488510731126445485_4130863203_100
  // Effect:
  //   Will override field 1654523497 in the form with form signature
  //   10011880710453506489 to 3 and field 4130863203 in the form with
  //   alternative form signature 10488510731126445485 to 100.
  kSpec,
  // Overrides are given in a JSON format.
  //
  // The following is an example for two forms, each of which has two fields:
  // - The form with signature 12345 has has fields with signatures 123 and 456.
  // - The form with signature 67890 has has two fields with signature 789.
  //
  //   {
  //     "12345": {
  //        "123": [ {
  //          "predictions": ["NAME_FIRST"]
  //        } ],
  //        "456": [ {
  //          "predictions": ["NAME_FIRST", "PASSPORT_NUMBER"]
  //        } ]
  //     },
  //     "67890": {
  //        "789": [ {
  //          "predictions": ["PASSPORT_ISSUE_DATE"],
  //          "format_string_type": "DATE",
  //          "format_string": "YYYY-MM-DD"
  //        }, {
  //          "predictions": [172],
  //          "format_string_type": "DATE",
  //          "format_string": "DD/MM/YYYY"
  //        } ]
  //     }
  //   }
  kJson
};

// Attempts to parse an override specification string into
// `ServerPredictionOverrides`.
//
// These overrides can be specified as follows:
//
//   chrome --enable-features=AutofillOverridePredictions:spec/SPECIFICATION
//   chrome --enable-features=AutofillOverridePredictions:json/BASE64_JSON
//
// where SPECIFICATION is a string as documented in OverrideFormat::kSpec and
// BASE64_JSON is a Base64-encoded JSON dict as documented in
// OverrideFormat::kJson.
base::expected<ServerPredictionOverrides, std::string>
ParseServerPredictionOverrides(std::string_view overrides_as_string,
                               OverrideFormat format);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_SERVER_PREDICTION_OVERRIDES_H_
