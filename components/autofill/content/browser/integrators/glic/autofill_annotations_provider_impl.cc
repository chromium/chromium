// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/glic/autofill_annotations_provider_impl.h"

#include "base/containers/map_util.h"
#include "base/functional/function_ref.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace optimization_guide {

using autofill::AutofillClient;
using autofill::AutofillField;
using autofill::AutofillManager;
using autofill::ContentAutofillDriver;
using autofill::DenseSet;
using autofill::FieldGlobalId;
using autofill::FieldRendererId;
using autofill::FieldType;
using autofill::FieldTypeGroup;
using autofill::FieldTypeSet;
using autofill::FormGlobalId;
using autofill::FormStructure;
using autofill::FormType;
using autofill::LocalFrameToken;
using autofill::PersonalDataManager;
using autofill::ValuablesDataManager;

namespace {

// Key for `base::SupportsUserData` aspects of `SectionMapping`.
const void* const kSectionMappingKey = &kSectionMappingKey;

std::string FormGlobalIdToString(FormGlobalId form_id) {
  return base::StrCat({form_id.frame_token.ToString(), "_",
                       base::NumberToString(form_id.renderer_id.value())});
}

class SectionMapping : public base::SupportsUserData::Data {
 public:
  SectionMapping();
  SectionMapping(const SectionMapping&) = delete;
  SectionMapping& operator=(const SectionMapping&) = delete;
  ~SectionMapping() override;

  static SectionMapping* GetInstance(
      ConvertAIPageContentToProtoSession& session);

  uint32_t GetOrCreateSectionIdentifier(const FormGlobalId& form_id,
                                        const std::string& section);

 private:
  base::flat_map<std::string, uint32_t> autofill_section_numbers_;
};

SectionMapping::SectionMapping() = default;
SectionMapping::~SectionMapping() = default;

// static
SectionMapping* SectionMapping::GetInstance(
    ConvertAIPageContentToProtoSession& session) {
  SectionMapping* mapping =
      static_cast<SectionMapping*>(session.GetUserData(kSectionMappingKey));
  if (!mapping) {
    auto new_mapping = std::make_unique<SectionMapping>();
    mapping = new_mapping.get();
    session.SetUserData(kSectionMappingKey, std::move(new_mapping));
  }
  return mapping;
}

uint32_t SectionMapping::GetOrCreateSectionIdentifier(
    const FormGlobalId& form_id,
    const std::string& section) {
  // Because different forms can have the same section titles, we use
  // (form_id, section) as the key for `section_numbers`.
  const std::string section_id = FormGlobalIdToString(form_id) + section;

  // Find the current section or create a new one.
  auto iter = autofill_section_numbers_.find(section_id);
  if (iter == autofill_section_numbers_.end()) {
    iter = autofill_section_numbers_
               .emplace(section_id, autofill_section_numbers_.size())
               .first;
  }
  return iter->second;
}

const FormStructure* GetAutofillForm(
    content::RenderFrameHost& render_frame_host,
    const FieldGlobalId& field_global_id) {
  content::WebContents& web_contents =
      *content::WebContents::FromRenderFrameHost(&render_frame_host);

  // Use the `ContentAutofillDriver` of the main frame because forms are
  // flattened and propagated into the primary main frame `AutofillManager`.
  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(
          web_contents.GetPrimaryMainFrame());
  if (!autofill_driver) {
    return nullptr;
  }

  AutofillManager& autofill_manager = autofill_driver->GetAutofillManager();
  return autofill_manager.FindCachedFormById(field_global_id);
}

// Returns the redaction reason for a given Autofill `field_type`. Note that
// some field types return 'no redaction needed' from this method only because a
// decision has not yet been made on them.
AutofillFieldRedactionReason GetRedactionReason(FieldType field_type) {
  switch (field_type) {
    // We should not redact cases where we have not identified the field type.
    case autofill::NO_SERVER_DATA:
    case autofill::UNKNOWN_TYPE:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // Names are not redacted.
    case autofill::NAME_FIRST:
    case autofill::NAME_MIDDLE:
    case autofill::NAME_LAST:
    case autofill::NAME_MIDDLE_INITIAL:
    case autofill::NAME_FULL:
    case autofill::NAME_SUFFIX:
    case autofill::NAME_LAST_FIRST:
    case autofill::NAME_LAST_CONJUNCTION:
    case autofill::NAME_LAST_SECOND:
    case autofill::NAME_HONORIFIC_PREFIX:
    case autofill::ALTERNATIVE_FULL_NAME:
    case autofill::ALTERNATIVE_GIVEN_NAME:
    case autofill::ALTERNATIVE_FAMILY_NAME:
    case autofill::NAME_LAST_PREFIX:
    case autofill::NAME_LAST_CORE:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // Email address is not redacted.
    case autofill::EMAIL_ADDRESS:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // Cardholder name is not redacted.
    case autofill::CREDIT_CARD_NAME_FULL:
    case autofill::CREDIT_CARD_NAME_FIRST:
    case autofill::CREDIT_CARD_NAME_LAST:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // Other credit card data is redacted.
    case autofill::CREDIT_CARD_NUMBER:
    case autofill::CREDIT_CARD_EXP_MONTH:
    case autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case autofill::CREDIT_CARD_TYPE:
    case autofill::CREDIT_CARD_VERIFICATION_CODE:
    case autofill::CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return AutofillFieldRedactionReason::kShouldRedactForPayments;

    // Password fields have already been redacted on the renderer side.
    case autofill::PASSWORD:
    case autofill::ACCOUNT_CREATION_PASSWORD:
    case autofill::NOT_ACCOUNT_CREATION_PASSWORD:
    case autofill::USERNAME:
    case autofill::USERNAME_AND_EMAIL_ADDRESS:
    case autofill::NEW_PASSWORD:
    case autofill::PROBABLY_NEW_PASSWORD:
    case autofill::NOT_NEW_PASSWORD:
    case autofill::CONFIRMATION_PASSWORD:
    case autofill::SINGLE_USERNAME:
    case autofill::SINGLE_USERNAME_FORGOT_PASSWORD:
    case autofill::SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // The following fields have not yet been decided upon. By default they are
    // considered non-sensitive to avoid over-redacting. Any item here may
    // change from `AutofillFieldRedactionReason::kNoRedactionNeeded` to `true`
    // in the future.
    case autofill::ADDRESS_HOME_LINE1:
    case autofill::ADDRESS_HOME_LINE2:
    case autofill::ADDRESS_HOME_APT_NUM:
    case autofill::ADDRESS_HOME_CITY:
    case autofill::ADDRESS_HOME_STATE:
    case autofill::ADDRESS_HOME_ZIP:
    case autofill::ADDRESS_HOME_COUNTRY:
    case autofill::ADDRESS_HOME_STREET_ADDRESS:
    case autofill::ADDRESS_HOME_SORTING_CODE:
    case autofill::ADDRESS_HOME_DEPENDENT_LOCALITY:
    case autofill::ADDRESS_HOME_LINE3:
    case autofill::ADDRESS_HOME_STREET_NAME:
    case autofill::ADDRESS_HOME_HOUSE_NUMBER:
    case autofill::ADDRESS_HOME_SUBPREMISE:
    case autofill::ADDRESS_HOME_OTHER_SUBUNIT:
    case autofill::ADDRESS_HOME_ADDRESS:
    case autofill::ADDRESS_HOME_ADDRESS_WITH_NAME:
    case autofill::ADDRESS_HOME_FLOOR:
    case autofill::ADDRESS_HOME_OVERFLOW:
    case autofill::ADDRESS_HOME_LANDMARK:
    case autofill::ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
    case autofill::ADDRESS_HOME_ADMIN_LEVEL2:
    case autofill::ADDRESS_HOME_STREET_LOCATION:
    case autofill::ADDRESS_HOME_BETWEEN_STREETS:
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
    case autofill::ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
    case autofill::ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
    case autofill::ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_1:
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_2:
    case autofill::ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
    case autofill::ADDRESS_HOME_APT:
    case autofill::ADDRESS_HOME_APT_TYPE:
    case autofill::ADDRESS_HOME_ZIP_PREFIX:
    case autofill::ADDRESS_HOME_ZIP_SUFFIX:
    case autofill::PHONE_HOME_NUMBER:
    case autofill::PHONE_HOME_CITY_CODE:
    case autofill::PHONE_HOME_COUNTRY_CODE:
    case autofill::PHONE_HOME_CITY_AND_NUMBER:
    case autofill::PHONE_HOME_WHOLE_NUMBER:
    case autofill::PHONE_HOME_EXTENSION:
    case autofill::PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case autofill::PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case autofill::PHONE_HOME_NUMBER_PREFIX:
    case autofill::PHONE_HOME_NUMBER_SUFFIX:
    case autofill::COMPANY_NAME:
    case autofill::MERCHANT_EMAIL_SIGNUP:
    case autofill::MERCHANT_PROMO_CODE:
    case autofill::AMBIGUOUS_TYPE:
    case autofill::SEARCH_TERM:
    case autofill::PRICE:
    case autofill::NOT_PASSWORD:
    case autofill::NOT_USERNAME:
    case autofill::IBAN_VALUE:
    case autofill::NUMERIC_QUANTITY:
    case autofill::ONE_TIME_CODE:
    case autofill::DELIVERY_INSTRUCTIONS:
    case autofill::LOYALTY_MEMBERSHIP_ID:
    case autofill::PASSPORT_NUMBER:
    case autofill::PASSPORT_ISSUING_COUNTRY:
    case autofill::PASSPORT_EXPIRATION_DATE:
    case autofill::PASSPORT_ISSUE_DATE:
    case autofill::LOYALTY_MEMBERSHIP_PROGRAM:
    case autofill::LOYALTY_MEMBERSHIP_PROVIDER:
    case autofill::VEHICLE_LICENSE_PLATE:
    case autofill::VEHICLE_VIN:
    case autofill::VEHICLE_MAKE:
    case autofill::VEHICLE_MODEL:
    case autofill::DRIVERS_LICENSE_REGION:
    case autofill::DRIVERS_LICENSE_NUMBER:
    case autofill::DRIVERS_LICENSE_EXPIRATION_DATE:
    case autofill::DRIVERS_LICENSE_ISSUE_DATE:
    case autofill::VEHICLE_YEAR:
    case autofill::VEHICLE_PLATE_STATE:
    case autofill::EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
    case autofill::NATIONAL_ID_CARD_NUMBER:
    case autofill::NATIONAL_ID_CARD_EXPIRATION_DATE:
    case autofill::NATIONAL_ID_CARD_ISSUE_DATE:
    case autofill::NATIONAL_ID_CARD_ISSUING_COUNTRY:
    case autofill::KNOWN_TRAVELER_NUMBER:
    case autofill::KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
    case autofill::REDRESS_NUMBER:
    case autofill::FLIGHT_RESERVATION_FLIGHT_NUMBER:
    case autofill::FLIGHT_RESERVATION_CONFIRMATION_CODE:
    case autofill::FLIGHT_RESERVATION_TICKET_NUMBER:
    case autofill::FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
    case autofill::FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
    case autofill::FLIGHT_RESERVATION_DEPARTURE_DATE:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;

    // These cases are not produced by field classification, but have to be
    // handled so that the switch is complete.
    case autofill::EMPTY_TYPE:
    case autofill::MAX_VALID_FIELD_TYPE:
      return AutofillFieldRedactionReason::kNoRedactionNeeded;
  }
}

// Returns the AutofillFieldRedactionReason for a set of field types. If
// multiple field types require redacting, one reason will be chosen at random
// (based on set iteration order).
AutofillFieldRedactionReason GetRedactionReason(
    const FieldTypeSet& field_types) {
  for (const FieldType field_type : field_types) {
    AutofillFieldRedactionReason redaction_reason =
        GetRedactionReason(field_type);
    switch (redaction_reason) {
      case AutofillFieldRedactionReason::kShouldRedactForPayments:
        return redaction_reason;
      case AutofillFieldRedactionReason::kNoRedactionNeeded:
        continue;
    }
  }

  return AutofillFieldRedactionReason::kNoRedactionNeeded;
}

}  // namespace

AutofillAnnotationsProviderImpl::~AutofillAnnotationsProviderImpl() = default;

std::optional<AutofillFieldMetadata>
AutofillAnnotationsProviderImpl::GetAutofillFieldData(
    content::RenderFrameHost& render_frame_host,
    int32_t dom_node_id,
    ConvertAIPageContentToProtoSession& session) {
  // Determine `AutofillField` from Autofill.
  FieldGlobalId field_global_id = {
      LocalFrameToken(render_frame_host.GetFrameToken().value()),
      FieldRendererId(dom_node_id)};
  const FormStructure* form =
      GetAutofillForm(render_frame_host, field_global_id);
  if (!form) {
    return std::nullopt;
  }
  const AutofillField* field = form->GetFieldById(field_global_id);
  if (!field) {
    return std::nullopt;
  }

  AutofillFieldMetadata metadata;

  metadata.section_id =
      SectionMapping::GetInstance(session)->GetOrCreateSectionIdentifier(
          form->global_id(), field->section().ToString());

  const DenseSet<FormType>& form_types = field->Type().GetFormTypes();
  metadata.coarse_field_type = [&] {
    if (form_types.contains(FormType::kAddressForm)) {
      return proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS;
    } else if (form_types.contains(FormType::kCreditCardForm) ||
               form_types.contains(FormType::kStandaloneCvcForm)) {
      return proto::COARSE_AUTOFILL_FIELD_TYPE_CREDIT_CARD;
    }
    return proto::COARSE_AUTOFILL_FIELD_TYPE_UNSUPPORTED;
  }();

  metadata.redaction_reason = GetRedactionReason(field->Type().GetTypes());

  return metadata;
}

AutofillAvailability AutofillAnnotationsProviderImpl::GetAutofillAvailability(
    content::RenderFrameHost& render_frame_host) {
  content::WebContents& web_contents =
      *content::WebContents::FromRenderFrameHost(&render_frame_host);
  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(
          web_contents.GetPrimaryMainFrame());
  if (!autofill_driver) {
    return {};
  }

  AutofillClient& client = autofill_driver->GetAutofillClient();
  if (!client.HasPersonalDataManager()) {
    return {};
  }
  const PersonalDataManager& pdm = client.GetPersonalDataManager();

  return AutofillAvailability{
      .has_fillable_address = client.IsAutofillProfileEnabled() &&
                              !pdm.address_data_manager().GetProfiles().empty(),
      .has_fillable_credit_card =
          client.GetPaymentsAutofillClient()
              ->IsAutofillPaymentMethodsEnabled() &&
          !pdm.payments_data_manager().GetCreditCards().empty(),
  };
}

}  // namespace optimization_guide
