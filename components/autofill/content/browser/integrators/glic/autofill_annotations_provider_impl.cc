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
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace optimization_guide {

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

namespace {

struct FieldMetadata {
  // The `FormGlobalId` of the form that is associated with a `field` on the
  // main frame.
  FormGlobalId form_id;
  // The section identifier assigned to a field by autofill. This is scoped
  // to a specific form.
  std::string section;
  // The form types represented by a specific field.
  DenseSet<FormType> form_types;
};

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

// Returns metadata about a field identified by the `field_global_id` and the
// `RenderFrameHost` in which the field is located.
std::optional<FieldMetadata> GetFieldMetadata(
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
    return {};
  }

  AutofillManager& autofill_manager = autofill_driver->GetAutofillManager();
  const FormStructure* form =
      autofill_manager.FindCachedFormById(field_global_id);
  if (!form) {
    return {};
  }
  const AutofillField* field = form->GetFieldById(field_global_id);
  if (!field) {
    return {};
  }

  return FieldMetadata{
      .form_id = form->global_id(),
      .section = field->section().ToString(),
      .form_types = field->Type().GetFormTypes(),
  };
}

// Writes `autofill_section_id` and `coarse_autofill_field_type` into
// `form_control_data`.
void UpdateFormControlData(
    ConvertAIPageContentToProtoSession& session,
    const FieldMetadata& field_metadata,
    optimization_guide::proto::FormControlData* form_control_data) {
  // Update the section.
  form_control_data->set_autofill_section_id(
      SectionMapping::GetInstance(session)->GetOrCreateSectionIdentifier(
          field_metadata.form_id, field_metadata.section));

  // Update the coarse field type.
  const DenseSet<FormType>& form_types = field_metadata.form_types;
  proto::CoarseAutofillFieldType coarse_field_type = [&] {
    if (form_types.contains(FormType::kAddressForm)) {
      return proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS;
    } else if (form_types.contains(FormType::kCreditCardForm) ||
               form_types.contains(FormType::kStandaloneCvcForm)) {
      return proto::COARSE_AUTOFILL_FIELD_TYPE_CREDIT_CARD;
    }
    return proto::COARSE_AUTOFILL_FIELD_TYPE_UNSUPPORTED;
  }();
  form_control_data->add_coarse_autofill_field_type(coarse_field_type);
}

}  // namespace

AutofillAnnotationsProviderImpl::~AutofillAnnotationsProviderImpl() = default;

void AutofillAnnotationsProviderImpl::AddAutofillAnnotations(
    content::RenderFrameHost& render_frame_host,
    ConvertAIPageContentToProtoSession& session,
    optimization_guide::proto::ContentAttributes* content_attributes) {
  CHECK(content_attributes->has_form_control_data());
  optimization_guide::proto::FormControlData* form_control_data =
      content_attributes->mutable_form_control_data();

  // Determine `AutofillField` from Autofill.
  FieldGlobalId field_global_id = {
      LocalFrameToken(render_frame_host.GetFrameToken().value()),
      FieldRendererId(content_attributes->common_ancestor_dom_node_id())};
  std::optional<FieldMetadata> field_metadata =
      GetFieldMetadata(render_frame_host, field_global_id);
  if (!field_metadata) {
    return;
  }

  UpdateFormControlData(session, *field_metadata, form_control_data);
}

}  // namespace optimization_guide
