// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AUTOFILL_ANNOTATIONS_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AUTOFILL_ANNOTATIONS_PROVIDER_H_

#include "base/supports_user_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace optimization_guide {
class ConvertAIPageContentToProtoSession;
}  // namespace optimization_guide

namespace optimization_guide {

// Tracks reasons that Autofill thinks a given field should be redacted.
enum class AutofillFieldRedactionReason {
  // This field does not need to be redacted according to Autofill.
  kNoRedactionNeeded,

  // This field should be redacted as it could contain sensitive payments
  // information.
  kShouldRedactForPayments,
};

// Represents information derived from Autofill for a given field.
struct AutofillFieldMetadata {
  // The coarse type of the form that the field belongs to.
  proto::CoarseAutofillFieldType coarse_field_type;

  // An identifier of a section of a form that this field belongs to. Form
  // controls with the same `section_id` are filled together by autofill.
  //
  // A single form can consist of multiple sections (e.g. billing and shipping).
  // Two forms will have generally disjoined `section_id`s - except for
  // flattened forms, where one virtual form is built by combining forms from
  // multiple iframes.
  uint32_t section_id;

  // The redaction reason that Autofill suggests for the field. Note that this
  // is based only on the detected type of the field, not whether or not it
  // contains any actual content.
  AutofillFieldRedactionReason redaction_reason =
      AutofillFieldRedactionReason::kNoRedactionNeeded;
};

// Represents information about what fillable data is available from Autofill.
struct AutofillAvailability {
  // Whether or not there is an address profile available to fill. The profile
  // may or may not be complete.
  bool has_fillable_address = false;

  // Whether or not there is a credit card available to fill. The credit card
  // data may or may not be complete.
  bool has_fillable_credit_card = false;
};

// This interface enables integrating Autofill information with the Annotated
// Page Contents for a given form control.
class AutofillAnnotationsProvider : public base::SupportsUserData::Data {
 public:
  AutofillAnnotationsProvider() = default;
  ~AutofillAnnotationsProvider() override = default;

  // Gets the `AutofillAnnotationsProvider` for the given `WebContents`.
  static AutofillAnnotationsProvider* GetFor(
      content::WebContents* web_contents);

  // Sets the `AutofillAnnotationsProvider` for the given `WebContents`. Takes
  // ownership of the provider.
  static void SetFor(content::WebContents* web_contents,
                     std::unique_ptr<AutofillAnnotationsProvider> provider);

  // Returns Autofill-derived data, if any, for the Autofill field corresponding
  // to the form control node represented by `dom_node_id`.
  //
  // `render_frame_host` needs to be the RFH that contains the form control.
  virtual std::optional<AutofillFieldMetadata> GetAutofillFieldData(
      content::RenderFrameHost& render_frame_host,
      int32_t dom_node_id,
      ConvertAIPageContentToProtoSession& session) = 0;

  // Returns data from Autofill as to what data is available to fill at the
  // current time.
  //
  // `render_frame_host` is used to lookup the Autofill PersonalDataManager.
  // The PersonalDataManager is a one-per-profile concept despite us passing in
  // a `render_frame_host` here; the `render_frame_host` is just a simple way to
  // get to the PersonalDataManager via existing APIs.
  virtual AutofillAvailability GetAutofillAvailability(
      content::RenderFrameHost& render_frame_host) = 0;

 private:
  // The key for storing the `AutofillAnnotationsProvider` in a `WebContents`.
  static const void* UserDataKey();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AUTOFILL_ANNOTATIONS_PROVIDER_H_
