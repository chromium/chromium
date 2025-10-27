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
          client.IsAutofillPaymentMethodsEnabled() &&
          !pdm.payments_data_manager().GetCreditCards().empty(),
  };
}

}  // namespace optimization_guide
