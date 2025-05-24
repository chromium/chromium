// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_ENCODING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_ENCODING_H_

#include <optional>
#include <string>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/crowdsourcing/randomized_encoder.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

// Specifies if the Username First Flow vote has intermediate values.
enum class IsMostRecentSingleUsernameCandidate {
  // Field is not part of Username First Flow.
  kNotPartOfUsernameFirstFlow = 0,
  // Field is candidate for username in Username First Flow and has no
  // intermediate fields
  kMostRecentCandidate = 1,
  // Field is candidate for username in Username First Flow and has intermediate
  // fields between candidate and password form.
  kHasIntermediateValuesInBetween = 2,
};

struct EncodeUploadRequestOptions {
  struct Field {
    Field();
    Field(const Field&&) = delete;
    Field(Field&&);
    Field& operator=(const Field&&) = delete;
    Field& operator=(Field&&);
    ~Field();

    // All date format strings that match the field value.
    base::flat_set<std::u16string> format_strings;

    // Strength of the single username vote signal, if applicable.
    std::optional<AutofillUploadContents::Field::SingleUsernameVoteType>
        single_username_vote_type;

    // If set to `kMostRecentCandidate`, the field is candidate for username
    // in Username First Flow and the field has no intermediate
    // fields (like OTP/Captcha) between the candidate and the password form.
    // If set to `kHasIntermediateValuesInBetween`, the field is candidate for
    // username in Username First Flow, but has intermediate fields between the
    // candidate and the password form.
    // If set to `kNotPartOfUsernameFirstFlow`, the field is not part of
    // Username First Flow.
    IsMostRecentSingleUsernameCandidate
        is_most_recent_single_username_candidate =
            IsMostRecentSingleUsernameCandidate::kNotPartOfUsernameFirstFlow;

    // The type of password generation event, if it happened.
    AutofillUploadContents::Field::PasswordGenerationType generation_type =
        AutofillUploadContents::Field::NO_GENERATION;

    // Whether the generated password was changed by user.
    bool generated_password_changed = false;

    // For username fields, a low-entropy hash of the field's initial value
    // before user-interactions or automatic fillings. This field is used to
    // detect static placeholders. On non-username fields, it is not set.
    std::optional<uint32_t> initial_value_hash;

    // The vote type, if the autofill type is USERNAME or any password vote.
    // Otherwise, the field is ignored. `vote_type` provides context as to what
    // triggered the vote.
    AutofillUploadContents::Field::VoteType vote_type =
        AutofillUploadContents::Field::NO_INFORMATION;
  };

  EncodeUploadRequestOptions();
  EncodeUploadRequestOptions(const EncodeUploadRequestOptions&) = delete;
  EncodeUploadRequestOptions(EncodeUploadRequestOptions&&);
  EncodeUploadRequestOptions& operator=(const EncodeUploadRequestOptions&) =
      delete;
  EncodeUploadRequestOptions& operator=(EncodeUploadRequestOptions&&);
  ~EncodeUploadRequestOptions();

  // The randomized encoder to use to encode form metadata during upload.
  // If this is nullptr, no randomized metadata is sent.
  std::optional<RandomizedEncoder> encoder;

  // The type of the event that was taken as an indication that the form has
  // been successfully submitted.
  mojom::SubmissionIndicatorEvent submission_event =
      mojom::SubmissionIndicatorEvent::NONE;

  // The signatures of forms recently submitted on the same origin within a
  // small period of time.
  FormStructure::FormAssociations form_associations;

  FieldTypeSet available_field_types;

  std::optional<FormSignature> login_form_signature;

  bool observed_submission = false;

  std::map<FieldGlobalId, Field> fields;
};

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
    const EncodeUploadRequestOptions& options);

// Encodes the list of `forms` and their fields that are valid into an
// `AutofillPageQueryRequest` proto. The queried FormSignatures and
// FieldSignatures are also returned in the same order as in `query`. In case
// multiple FormStructures have the same FormSignature, only the first one is
// included in `AutofillPageQueryRequest` and the returned queried form
// signatures.
std::pair<AutofillPageQueryRequest, std::vector<FormSignature>>
EncodeAutofillPageQueryRequest(
    const std::vector<raw_ptr<const FormStructure, VectorExperimental>>& forms);

// Parses `payload` as AutofillQueryResponse proto and calls
// `ProcessServerPredictionsQueryResponse`.
void ParseServerPredictionsQueryResponse(
    std::string_view payload,
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    LogManager* log_manager);

// Parses the field types from the server query response. `forms` must be the
// same as the one passed to `EncodeAutofillPageQueryRequest()` when
// constructing the query.
void ProcessServerPredictionsQueryResponse(
    const AutofillQueryResponse& response,
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    LogManager* log_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_ENCODING_H_
