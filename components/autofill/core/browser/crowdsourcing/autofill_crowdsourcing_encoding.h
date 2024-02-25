// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_ENCODING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_ENCODING_H_

#include <string>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"

namespace autofill {

// Encodes the given FormStructure as a vector of protobufs.
//
// On success, the returned vector is non-empty. The first element encodes the
// entire FormStructure. In some cases, a `login_form_signature` is included
// as part of the upload. This field is empty when sending upload requests for
// non-login forms.
//
// If the FormStructure is a frame-transcending form, there may be additional
// AutofillUploadContents elements in the vector, which encode the renderer
// forms (see below for an explanation). These elements omit the renderer
// form's metadata because retrieving this would require significant plumbing
// from AutofillDriverRouter.
//
// The renderer forms are the forms that constitute a frame-transcending form.
// AutofillDriverRouter receives these forms from the renderer and flattens
// them into a single fresh form. Only the latter form is exposed to the rest
// of the browser process. For server predictions, however, we want to query
// and upload also votes also for the signatures of the renderer forms. For
// example, the frame-transcending form
//   <form id=1>
//     <input autocomplete="cc-name">
//     <iframe>
//       #document
//         <form id=2>
//           <input autocomplete="cc-number">
//         </form>
//     </iframe>
//   </form>
// is flattened into a single form that contains the cc-name and cc-number
// fields. We want to vote for this flattened form as well as for the original
// form signatures of forms 1 and 2.
std::vector<AutofillUploadContents> EncodeUploadRequest(
    const FormStructure& form,
    const FieldTypeSet& available_field_types,
    std::string_view login_form_signature,
    bool observed_submission);

// Encodes the list of `forms` and their fields that are valid into an
// `AutofillPageQueryRequest` proto. The queried FormSignatures and
// FieldSignatures are also returned in the same order as in `query`. In case
// multiple FormStructures have the same FormSignature, only the first one is
// included in `AutofillPageQueryRequest` and the returned queried form
// signatures.
std::pair<AutofillPageQueryRequest, std::vector<FormSignature>>
EncodeAutofillPageQueryRequest(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms);

// Parses `payload` as AutofillQueryResponse proto and calls
// `ProcessServerPredictionsQueryResponse`.
void ParseServerPredictionsQueryResponse(
    std::string_view payload,
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager);

// Parses the field types from the server query response. `forms` must be the
// same as the one passed to `EncodeAutofillPageQueryRequest()` when
// constructing the query. `form_interactions_ukm_logger` is used to provide
// logs to UKM and can be null in tests.
void ProcessServerPredictionsQueryResponse(
    const AutofillQueryResponse& response,
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_ENCODING_H_
