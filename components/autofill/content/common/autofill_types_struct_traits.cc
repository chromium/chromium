// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/common/autofill_types_struct_traits.h"

#include "base/i18n/rtl.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
autofill::mojom::CheckStatus
EnumTraits<autofill::mojom::CheckStatus, autofill::FormFieldData::CheckStatus>::
    ToMojom(autofill::FormFieldData::CheckStatus input) {
  switch (input) {
    case autofill::FormFieldData::CheckStatus::NOT_CHECKABLE:
      return autofill::mojom::CheckStatus::NOT_CHECKABLE;
    case autofill::FormFieldData::CheckStatus::CHECKABLE_BUT_UNCHECKED:
      return autofill::mojom::CheckStatus::CHECKABLE_BUT_UNCHECKED;
    case autofill::FormFieldData::CheckStatus::CHECKED:
      return autofill::mojom::CheckStatus::CHECKED;
  }

  NOTREACHED();
  return autofill::mojom::CheckStatus::NOT_CHECKABLE;
}

// static
bool EnumTraits<autofill::mojom::CheckStatus,
                autofill::FormFieldData::CheckStatus>::
    FromMojom(autofill::mojom::CheckStatus input,
              autofill::FormFieldData::CheckStatus* output) {
  switch (input) {
    case autofill::mojom::CheckStatus::NOT_CHECKABLE:
      *output = autofill::FormFieldData::CheckStatus::NOT_CHECKABLE;
      return true;
    case autofill::mojom::CheckStatus::CHECKABLE_BUT_UNCHECKED:
      *output = autofill::FormFieldData::CheckStatus::CHECKABLE_BUT_UNCHECKED;
      return true;
    case autofill::mojom::CheckStatus::CHECKED:
      *output = autofill::FormFieldData::CheckStatus::CHECKED;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
autofill::mojom::RoleAttribute
EnumTraits<autofill::mojom::RoleAttribute,
           autofill::FormFieldData::RoleAttribute>::
    ToMojom(autofill::FormFieldData::RoleAttribute input) {
  switch (input) {
    case autofill::FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION:
      return autofill::mojom::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION;
    case autofill::FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_OTHER:
      return autofill::mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
  }

  NOTREACHED();
  return autofill::mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
}

// static
bool EnumTraits<autofill::mojom::RoleAttribute,
                autofill::FormFieldData::RoleAttribute>::
    FromMojom(autofill::mojom::RoleAttribute input,
              autofill::FormFieldData::RoleAttribute* output) {
  switch (input) {
    case autofill::mojom::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION:
      *output =
          autofill::FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_PRESENTATION;
      return true;
    case autofill::mojom::RoleAttribute::ROLE_ATTRIBUTE_OTHER:
      *output = autofill::FormFieldData::RoleAttribute::ROLE_ATTRIBUTE_OTHER;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
autofill::mojom::GenerationUploadStatus
EnumTraits<autofill::mojom::GenerationUploadStatus,
           autofill::PasswordForm::GenerationUploadStatus>::
    ToMojom(autofill::PasswordForm::GenerationUploadStatus input) {
  switch (input) {
    case autofill::PasswordForm::GenerationUploadStatus::NO_SIGNAL_SENT:
      return autofill::mojom::GenerationUploadStatus::NO_SIGNAL_SENT;
    case autofill::PasswordForm::GenerationUploadStatus::POSITIVE_SIGNAL_SENT:
      return autofill::mojom::GenerationUploadStatus::POSITIVE_SIGNAL_SENT;
    case autofill::PasswordForm::GenerationUploadStatus::NEGATIVE_SIGNAL_SENT:
      return autofill::mojom::GenerationUploadStatus::NEGATIVE_SIGNAL_SENT;
    case autofill::PasswordForm::GenerationUploadStatus::UNKNOWN_STATUS:
      return autofill::mojom::GenerationUploadStatus::UNKNOWN_STATUS;
  }

  NOTREACHED();
  return autofill::mojom::GenerationUploadStatus::UNKNOWN_STATUS;
}

// static
bool EnumTraits<autofill::mojom::GenerationUploadStatus,
                autofill::PasswordForm::GenerationUploadStatus>::
    FromMojom(autofill::mojom::GenerationUploadStatus input,
              autofill::PasswordForm::GenerationUploadStatus* output) {
  switch (input) {
    case autofill::mojom::GenerationUploadStatus::NO_SIGNAL_SENT:
      *output = autofill::PasswordForm::GenerationUploadStatus::NO_SIGNAL_SENT;
      return true;
    case autofill::mojom::GenerationUploadStatus::POSITIVE_SIGNAL_SENT:
      *output =
          autofill::PasswordForm::GenerationUploadStatus::POSITIVE_SIGNAL_SENT;
      return true;
    case autofill::mojom::GenerationUploadStatus::NEGATIVE_SIGNAL_SENT:
      *output =
          autofill::PasswordForm::GenerationUploadStatus::NEGATIVE_SIGNAL_SENT;
      return true;
    case autofill::mojom::GenerationUploadStatus::UNKNOWN_STATUS:
      *output = autofill::PasswordForm::GenerationUploadStatus::UNKNOWN_STATUS;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
autofill::mojom::PasswordFormType EnumTraits<
    autofill::mojom::PasswordFormType,
    autofill::PasswordForm::Type>::ToMojom(autofill::PasswordForm::Type input) {
  switch (input) {
    case autofill::PasswordForm::Type::TYPE_MANUAL:
      return autofill::mojom::PasswordFormType::TYPE_MANUAL;
    case autofill::PasswordForm::Type::TYPE_GENERATED:
      return autofill::mojom::PasswordFormType::TYPE_GENERATED;
    case autofill::PasswordForm::Type::TYPE_API:
      return autofill::mojom::PasswordFormType::TYPE_API;
  }

  NOTREACHED();
  return autofill::mojom::PasswordFormType::TYPE_MANUAL;
}

// static
bool EnumTraits<autofill::mojom::PasswordFormType,
                autofill::PasswordForm::Type>::
    FromMojom(autofill::mojom::PasswordFormType input,
              autofill::PasswordForm::Type* output) {
  switch (input) {
    case autofill::mojom::PasswordFormType::TYPE_MANUAL:
      *output = autofill::PasswordForm::Type::TYPE_MANUAL;
      return true;
    case autofill::mojom::PasswordFormType::TYPE_GENERATED:
      *output = autofill::PasswordForm::Type::TYPE_GENERATED;
      return true;
    case autofill::mojom::PasswordFormType::TYPE_API:
      *output = autofill::PasswordForm::Type::TYPE_API;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
autofill::mojom::PasswordFormScheme EnumTraits<
    autofill::mojom::PasswordFormScheme,
    autofill::PasswordForm::Scheme>::ToMojom(autofill::PasswordForm::Scheme
                                                 input) {
  switch (input) {
    case autofill::PasswordForm::Scheme::SCHEME_HTML:
      return autofill::mojom::PasswordFormScheme::SCHEME_HTML;
    case autofill::PasswordForm::Scheme::SCHEME_BASIC:
      return autofill::mojom::PasswordFormScheme::SCHEME_BASIC;
    case autofill::PasswordForm::Scheme::SCHEME_DIGEST:
      return autofill::mojom::PasswordFormScheme::SCHEME_DIGEST;
    case autofill::PasswordForm::Scheme::SCHEME_OTHER:
      return autofill::mojom::PasswordFormScheme::SCHEME_OTHER;
    case autofill::PasswordForm::Scheme::SCHEME_USERNAME_ONLY:
      return autofill::mojom::PasswordFormScheme::SCHEME_USERNAME_ONLY;
  }

  NOTREACHED();
  return autofill::mojom::PasswordFormScheme::SCHEME_OTHER;
}

// static
bool EnumTraits<autofill::mojom::PasswordFormScheme,
                autofill::PasswordForm::Scheme>::
    FromMojom(autofill::mojom::PasswordFormScheme input,
              autofill::PasswordForm::Scheme* output) {
  switch (input) {
    case autofill::mojom::PasswordFormScheme::SCHEME_HTML:
      *output = autofill::PasswordForm::Scheme::SCHEME_HTML;
      return true;
    case autofill::mojom::PasswordFormScheme::SCHEME_BASIC:
      *output = autofill::PasswordForm::Scheme::SCHEME_BASIC;
      return true;
    case autofill::mojom::PasswordFormScheme::SCHEME_DIGEST:
      *output = autofill::PasswordForm::Scheme::SCHEME_DIGEST;
      return true;
    case autofill::mojom::PasswordFormScheme::SCHEME_OTHER:
      *output = autofill::PasswordForm::Scheme::SCHEME_OTHER;
      return true;
    case autofill::mojom::PasswordFormScheme::SCHEME_USERNAME_ONLY:
      *output = autofill::PasswordForm::Scheme::SCHEME_USERNAME_ONLY;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
autofill::mojom::PasswordFormSubmissionIndicatorEvent
EnumTraits<autofill::mojom::PasswordFormSubmissionIndicatorEvent,
           autofill::PasswordForm::SubmissionIndicatorEvent>::
    ToMojom(autofill::PasswordForm::SubmissionIndicatorEvent input) {
  switch (input) {
    case autofill::PasswordForm::SubmissionIndicatorEvent::NONE:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::NONE;
    case autofill::PasswordForm::SubmissionIndicatorEvent::HTML_FORM_SUBMISSION:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::
          HTML_FORM_SUBMISSION;
    case autofill::PasswordForm::SubmissionIndicatorEvent::
        SAME_DOCUMENT_NAVIGATION:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::
          SAME_DOCUMENT_NAVIGATION;
    case autofill::PasswordForm::SubmissionIndicatorEvent::XHR_SUCCEEDED:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::
          XHR_SUCCEEDED;
    case autofill::PasswordForm::SubmissionIndicatorEvent::FRAME_DETACHED:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::
          FRAME_DETACHED;
    case autofill::PasswordForm::SubmissionIndicatorEvent::
        DOM_MUTATION_AFTER_XHR:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::
          DOM_MUTATION_AFTER_XHR;
    case autofill::PasswordForm::SubmissionIndicatorEvent::
        PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::
          PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD;
    case autofill::PasswordForm::SubmissionIndicatorEvent::
        DEPRECATED_MANUAL_SAVE:
    case autofill::PasswordForm::SubmissionIndicatorEvent::
        DEPRECATED_FILLED_FORM_ON_START_PROVISIONAL_LOAD:
    case autofill::PasswordForm::SubmissionIndicatorEvent::
        DEPRECATED_FILLED_INPUT_ELEMENTS_ON_START_PROVISIONAL_LOAD:
      break;
    case autofill::PasswordForm::SubmissionIndicatorEvent::
        SUBMISSION_INDICATOR_EVENT_COUNT:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::
          SUBMISSION_INDICATOR_EVENT_COUNT;
    case autofill::PasswordForm::SubmissionIndicatorEvent::
        PROBABLE_FORM_SUBMISSION:
      return autofill::mojom::PasswordFormSubmissionIndicatorEvent::
          PROBABLE_FORM_SUBMISSION;
  }

  NOTREACHED();
  return autofill::mojom::PasswordFormSubmissionIndicatorEvent::NONE;
}

// static
bool EnumTraits<autofill::mojom::PasswordFormSubmissionIndicatorEvent,
                autofill::PasswordForm::SubmissionIndicatorEvent>::
    FromMojom(autofill::mojom::PasswordFormSubmissionIndicatorEvent input,
              autofill::PasswordForm::SubmissionIndicatorEvent* output) {
  switch (input) {
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::NONE:
      *output = autofill::PasswordForm::SubmissionIndicatorEvent::NONE;
      return true;
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::
        HTML_FORM_SUBMISSION:
      *output = autofill::PasswordForm::SubmissionIndicatorEvent::
          HTML_FORM_SUBMISSION;
      return true;
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::
        SAME_DOCUMENT_NAVIGATION:
      *output = autofill::PasswordForm::SubmissionIndicatorEvent::
          SAME_DOCUMENT_NAVIGATION;
      return true;
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::XHR_SUCCEEDED:
      *output = autofill::PasswordForm::SubmissionIndicatorEvent::XHR_SUCCEEDED;
      return true;
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::FRAME_DETACHED:
      *output =
          autofill::PasswordForm::SubmissionIndicatorEvent::FRAME_DETACHED;
      return true;
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::
        DOM_MUTATION_AFTER_XHR:
      *output = autofill::PasswordForm::SubmissionIndicatorEvent::
          DOM_MUTATION_AFTER_XHR;
      return true;
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::
        PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD:
      *output = autofill::PasswordForm::SubmissionIndicatorEvent::
          PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD;
      return true;
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::
        PROBABLE_FORM_SUBMISSION:
      *output = autofill::PasswordForm::SubmissionIndicatorEvent::
          PROBABLE_FORM_SUBMISSION;
      return true;
    case autofill::mojom::PasswordFormSubmissionIndicatorEvent::
        SUBMISSION_INDICATOR_EVENT_COUNT:
      *output = autofill::PasswordForm::SubmissionIndicatorEvent::
          SUBMISSION_INDICATOR_EVENT_COUNT;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
autofill::mojom::PasswordFormFieldPredictionType
EnumTraits<autofill::mojom::PasswordFormFieldPredictionType,
           autofill::PasswordFormFieldPredictionType>::
    ToMojom(autofill::PasswordFormFieldPredictionType input) {
  switch (input) {
    case autofill::PasswordFormFieldPredictionType::PREDICTION_USERNAME:
      return autofill::mojom::PasswordFormFieldPredictionType::
          PREDICTION_USERNAME;
    case autofill::PasswordFormFieldPredictionType::PREDICTION_CURRENT_PASSWORD:
      return autofill::mojom::PasswordFormFieldPredictionType::
          PREDICTION_CURRENT_PASSWORD;
    case autofill::PasswordFormFieldPredictionType::PREDICTION_NEW_PASSWORD:
      return autofill::mojom::PasswordFormFieldPredictionType::
          PREDICTION_NEW_PASSWORD;
    case autofill::PasswordFormFieldPredictionType::PREDICTION_NOT_PASSWORD:
      return autofill::mojom::PasswordFormFieldPredictionType::
          PREDICTION_NOT_PASSWORD;
  }

  NOTREACHED();
  return autofill::mojom::PasswordFormFieldPredictionType::
      PREDICTION_NOT_PASSWORD;
}

// static
bool EnumTraits<autofill::mojom::PasswordFormFieldPredictionType,
                autofill::PasswordFormFieldPredictionType>::
    FromMojom(autofill::mojom::PasswordFormFieldPredictionType input,
              autofill::PasswordFormFieldPredictionType* output) {
  switch (input) {
    case autofill::mojom::PasswordFormFieldPredictionType::PREDICTION_USERNAME:
      *output = autofill::PasswordFormFieldPredictionType::PREDICTION_USERNAME;
      return true;
    case autofill::mojom::PasswordFormFieldPredictionType::
        PREDICTION_CURRENT_PASSWORD:
      *output = autofill::PasswordFormFieldPredictionType::
          PREDICTION_CURRENT_PASSWORD;
      return true;
    case autofill::mojom::PasswordFormFieldPredictionType::
        PREDICTION_NEW_PASSWORD:
      *output =
          autofill::PasswordFormFieldPredictionType::PREDICTION_NEW_PASSWORD;
      return true;
    case autofill::mojom::PasswordFormFieldPredictionType::
        PREDICTION_NOT_PASSWORD:
      *output =
          autofill::PasswordFormFieldPredictionType::PREDICTION_NOT_PASSWORD;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
autofill::mojom::SubmissionSource EnumTraits<
    autofill::mojom::SubmissionSource,
    autofill::SubmissionSource>::ToMojom(autofill::SubmissionSource input) {
  switch (input) {
    case autofill::SubmissionSource::NONE:
      return autofill::mojom::SubmissionSource::NONE;
    case autofill::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return autofill::mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION;
    case autofill::SubmissionSource::XHR_SUCCEEDED:
      return autofill::mojom::SubmissionSource::XHR_SUCCEEDED;
    case autofill::SubmissionSource::FRAME_DETACHED:
      return autofill::mojom::SubmissionSource::FRAME_DETACHED;
    case autofill::SubmissionSource::DOM_MUTATION_AFTER_XHR:
      return autofill::mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR;
    case autofill::SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return autofill::mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED;
    case autofill::SubmissionSource::FORM_SUBMISSION:
      return autofill::mojom::SubmissionSource::FORM_SUBMISSION;
  }
  NOTREACHED();
  return autofill::mojom::SubmissionSource::NONE;
}

// static
bool EnumTraits<autofill::mojom::SubmissionSource, autofill::SubmissionSource>::
    FromMojom(autofill::mojom::SubmissionSource input,
              autofill::SubmissionSource* output) {
  switch (input) {
    case autofill::mojom::SubmissionSource::NONE:
      *output = autofill::SubmissionSource::NONE;
      return true;
    case autofill::mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      *output = autofill::SubmissionSource::SAME_DOCUMENT_NAVIGATION;
      return true;
    case autofill::mojom::SubmissionSource::XHR_SUCCEEDED:
      *output = autofill::SubmissionSource::XHR_SUCCEEDED;
      return true;
    case autofill::mojom::SubmissionSource::FRAME_DETACHED:
      *output = autofill::SubmissionSource::FRAME_DETACHED;
      return true;
    case autofill::mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR:
      *output = autofill::SubmissionSource::DOM_MUTATION_AFTER_XHR;
      return true;
    case autofill::mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
      *output = autofill::SubmissionSource::PROBABLY_FORM_SUBMITTED;
      return true;
    case autofill::mojom::SubmissionSource::FORM_SUBMISSION:
      *output = autofill::SubmissionSource::FORM_SUBMISSION;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
autofill::mojom::LabelSource
EnumTraits<autofill::mojom::LabelSource, autofill::FormFieldData::LabelSource>::
    ToMojom(autofill::FormFieldData::LabelSource input) {
  switch (input) {
    case autofill::FormFieldData::LabelSource::UNKNOWN:
      return autofill::mojom::LabelSource::UNKNOWN;
    case autofill::FormFieldData::LabelSource::LABEL_TAG:
      return autofill::mojom::LabelSource::LABEL_TAG;
    case autofill::FormFieldData::LabelSource::P_TAG:
      return autofill::mojom::LabelSource::P_TAG;
    case autofill::FormFieldData::LabelSource::DIV_TABLE:
      return autofill::mojom::LabelSource::DIV_TABLE;
    case autofill::FormFieldData::LabelSource::TD_TAG:
      return autofill::mojom::LabelSource::TD_TAG;
    case autofill::FormFieldData::LabelSource::DD_TAG:
      return autofill::mojom::LabelSource::DD_TAG;
    case autofill::FormFieldData::LabelSource::LI_TAG:
      return autofill::mojom::LabelSource::LI_TAG;
    case autofill::FormFieldData::LabelSource::PLACE_HOLDER:
      return autofill::mojom::LabelSource::PLACE_HOLDER;
    case autofill::FormFieldData::LabelSource::ARIA_LABEL:
      return autofill::mojom::LabelSource::ARIA_LABEL;
    case autofill::FormFieldData::LabelSource::COMBINED:
      return autofill::mojom::LabelSource::COMBINED;
    case autofill::FormFieldData::LabelSource::VALUE:
      return autofill::mojom::LabelSource::VALUE;
  }

  NOTREACHED();
  return autofill::mojom::LabelSource::UNKNOWN;
}

// static
bool EnumTraits<autofill::mojom::LabelSource,
                autofill::FormFieldData::LabelSource>::
    FromMojom(autofill::mojom::LabelSource input,
              autofill::FormFieldData::LabelSource* output) {
  switch (input) {
    case autofill::mojom::LabelSource::UNKNOWN:
      *output = autofill::FormFieldData::LabelSource::UNKNOWN;
      return true;
    case autofill::mojom::LabelSource::LABEL_TAG:
      *output = autofill::FormFieldData::LabelSource::LABEL_TAG;
      return true;
    case autofill::mojom::LabelSource::P_TAG:
      *output = autofill::FormFieldData::LabelSource::P_TAG;
      return true;
    case autofill::mojom::LabelSource::DIV_TABLE:
      *output = autofill::FormFieldData::LabelSource::DIV_TABLE;
      return true;
    case autofill::mojom::LabelSource::TD_TAG:
      *output = autofill::FormFieldData::LabelSource::TD_TAG;
      return true;
    case autofill::mojom::LabelSource::DD_TAG:
      *output = autofill::FormFieldData::LabelSource::DD_TAG;
      return true;
    case autofill::mojom::LabelSource::LI_TAG:
      *output = autofill::FormFieldData::LabelSource::LI_TAG;
      return true;
    case autofill::mojom::LabelSource::PLACE_HOLDER:
      *output = autofill::FormFieldData::LabelSource::PLACE_HOLDER;
      return true;
    case autofill::mojom::LabelSource::ARIA_LABEL:
      *output = autofill::FormFieldData::LabelSource::ARIA_LABEL;
      return true;
    case autofill::mojom::LabelSource::COMBINED:
      *output = autofill::FormFieldData::LabelSource::COMBINED;
      return true;
    case autofill::mojom::LabelSource::VALUE:
      *output = autofill::FormFieldData::LabelSource::VALUE;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
autofill::mojom::FillingStatus
EnumTraits<autofill::mojom::FillingStatus, autofill::FillingStatus>::ToMojom(
    autofill::FillingStatus input) {
  switch (input) {
    case autofill::FillingStatus::SUCCESS:
      return autofill::mojom::FillingStatus::SUCCESS;
    case autofill::FillingStatus::ERROR_NO_VALID_FIELD:
      return autofill::mojom::FillingStatus::ERROR_NO_VALID_FIELD;
    case autofill::FillingStatus::ERROR_NOT_ALLOWED:
      return autofill::mojom::FillingStatus::ERROR_NOT_ALLOWED;
  }
  NOTREACHED();
  return autofill::mojom::FillingStatus::SUCCESS;
}

// static
bool EnumTraits<autofill::mojom::FillingStatus, autofill::FillingStatus>::
    FromMojom(autofill::mojom::FillingStatus input,
              autofill::FillingStatus* output) {
  switch (input) {
    case autofill::mojom::FillingStatus::SUCCESS:
      *output = autofill::FillingStatus::SUCCESS;
      return true;
    case autofill::mojom::FillingStatus::ERROR_NO_VALID_FIELD:
      *output = autofill::FillingStatus::ERROR_NO_VALID_FIELD;
      return true;
    case autofill::mojom::FillingStatus::ERROR_NOT_ALLOWED:
      *output = autofill::FillingStatus::ERROR_NOT_ALLOWED;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<
    autofill::mojom::FormFieldDataDataView,
    autofill::FormFieldData>::Read(autofill::mojom::FormFieldDataDataView data,
                                   autofill::FormFieldData* out) {
  if (!data.ReadLabel(&out->label))
    return false;
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadId(&out->id))
    return false;
  if (!data.ReadValue(&out->value))
    return false;

  if (!data.ReadFormControlType(&out->form_control_type))
    return false;
  if (!data.ReadAutocompleteAttribute(&out->autocomplete_attribute))
    return false;

  if (!data.ReadPlaceholder(&out->placeholder))
    return false;

  if (!data.ReadCssClasses(&out->css_classes))
    return false;

  if (!data.ReadSection(&out->section))
    return false;

  out->properties_mask = data.properties_mask();
  out->unique_renderer_id = data.unique_renderer_id();
  out->max_length = data.max_length();
  out->is_autofilled = data.is_autofilled();

  if (!data.ReadCheckStatus(&out->check_status))
    return false;

  out->is_focusable = data.is_focusable();
  out->should_autocomplete = data.should_autocomplete();

  if (!data.ReadRole(&out->role))
    return false;

  if (!data.ReadTextDirection(&out->text_direction))
    return false;

  out->is_enabled = data.is_enabled();
  out->is_readonly = data.is_readonly();
  if (!data.ReadValue(&out->typed_value))
    return false;

  if (!data.ReadOptionValues(&out->option_values))
    return false;
  if (!data.ReadOptionContents(&out->option_contents))
    return false;

  if (!data.ReadLabelSource(&out->label_source))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::FormDataDataView, autofill::FormData>::Read(
    autofill::mojom::FormDataDataView data,
    autofill::FormData* out) {
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadButtonTitle(&out->button_title))
    return false;
  if (!data.ReadOrigin(&out->origin))
    return false;
  if (!data.ReadAction(&out->action))
    return false;
  if (!data.ReadMainFrameOrigin(&out->main_frame_origin))
    return false;

  out->is_form_tag = data.is_form_tag();
  out->is_formless_checkout = data.is_formless_checkout();
  out->unique_renderer_id = data.unique_renderer_id();

  if (!data.ReadFields(&out->fields))
    return false;

  if (!data.ReadUsernamePredictions(&out->username_predictions))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::FormFieldDataPredictionsDataView,
                  autofill::FormFieldDataPredictions>::
    Read(autofill::mojom::FormFieldDataPredictionsDataView data,
         autofill::FormFieldDataPredictions* out) {
  if (!data.ReadField(&out->field))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadHeuristicType(&out->heuristic_type))
    return false;
  if (!data.ReadServerType(&out->server_type))
    return false;
  if (!data.ReadOverallType(&out->overall_type))
    return false;
  if (!data.ReadParseableName(&out->parseable_name))
    return false;
  if (!data.ReadSection(&out->section))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::FormDataPredictionsDataView,
                  autofill::FormDataPredictions>::
    Read(autofill::mojom::FormDataPredictionsDataView data,
         autofill::FormDataPredictions* out) {
  if (!data.ReadData(&out->data))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadFields(&out->fields))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordAndRealmDataView,
                  autofill::PasswordAndRealm>::
    Read(autofill::mojom::PasswordAndRealmDataView data,
         autofill::PasswordAndRealm* out) {
  if (!data.ReadPassword(&out->password))
    return false;
  if (!data.ReadRealm(&out->realm))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordFormFillDataDataView,
                  autofill::PasswordFormFillData>::
    Read(autofill::mojom::PasswordFormFillDataDataView data,
         autofill::PasswordFormFillData* out) {
  if (!data.ReadOrigin(&out->origin) || !data.ReadAction(&out->action) ||
      !data.ReadUsernameField(&out->username_field) ||
      !data.ReadPasswordField(&out->password_field) ||
      !data.ReadPreferredRealm(&out->preferred_realm) ||
      !data.ReadAdditionalLogins(&out->additional_logins))
    return false;

  out->form_renderer_id = data.form_renderer_id();
  out->wait_for_username = data.wait_for_username();
  out->has_renderer_ids = data.has_renderer_ids();
  out->username_may_use_prefilled_placeholder =
      data.username_may_use_prefilled_placeholder();

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordFormGenerationDataDataView,
                  autofill::PasswordFormGenerationData>::
    Read(autofill::mojom::PasswordFormGenerationDataDataView data,
         autofill::PasswordFormGenerationData* out) {
  out->form_signature = data.form_signature();
  out->field_signature = data.field_signature();
  if (data.has_confirmation_field()) {
    out->confirmation_field_signature.emplace(
        data.confirmation_field_signature());
  } else {
    DCHECK(!out->confirmation_field_signature);
  }
  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordGenerationUIDataDataView,
                  autofill::password_generation::PasswordGenerationUIData>::
    Read(autofill::mojom::PasswordGenerationUIDataDataView data,
         autofill::password_generation::PasswordGenerationUIData* out) {
  if (!data.ReadBounds(&out->bounds))
    return false;

  out->max_length = data.max_length();

  if (!data.ReadGenerationElement(&out->generation_element) ||
      !data.ReadTextDirection(&out->text_direction) ||
      !data.ReadPasswordForm(&out->password_form))
    return false;

  return true;
}

// static
bool StructTraits<
    autofill::mojom::PasswordFormDataView,
    autofill::PasswordForm>::Read(autofill::mojom::PasswordFormDataView data,
                                  autofill::PasswordForm* out) {
  if (!data.ReadScheme(&out->scheme) ||
      !data.ReadSignonRealm(&out->signon_realm) ||
      !data.ReadOriginWithPath(&out->origin) ||
      !data.ReadAction(&out->action) ||
      !data.ReadAffiliatedWebRealm(&out->affiliated_web_realm) ||
      !data.ReadSubmitElement(&out->submit_element) ||
      !data.ReadUsernameElement(&out->username_element) ||
      !data.ReadSubmissionEvent(&out->submission_event))
    return false;

  out->username_marked_by_site = data.username_marked_by_site();

  if (!data.ReadUsernameValue(&out->username_value) ||
      !data.ReadOtherPossibleUsernames(&out->other_possible_usernames) ||
      !data.ReadAllPossiblePasswords(&out->all_possible_passwords) ||
      !data.ReadPasswordElement(&out->password_element) ||
      !data.ReadPasswordValue(&out->password_value))
    return false;

  out->form_has_autofilled_value = data.form_has_autofilled_value();

  if (!data.ReadNewPasswordElement(&out->new_password_element) ||
      !data.ReadNewPasswordValue(&out->new_password_value))
    return false;

  out->new_password_marked_by_site = data.new_password_marked_by_site();

  if (!data.ReadConfirmationPasswordElement(
          &out->confirmation_password_element))
    return false;

  out->preferred = data.preferred();

  if (!data.ReadDateCreated(&out->date_created) ||
      !data.ReadDateSynced(&out->date_synced))
    return false;

  out->blacklisted_by_user = data.blacklisted_by_user();

  if (!data.ReadType(&out->type))
    return false;

  out->times_used = data.times_used();

  if (!data.ReadFormData(&out->form_data) ||
      !data.ReadGenerationUploadStatus(&out->generation_upload_status) ||
      !data.ReadDisplayName(&out->display_name) ||
      !data.ReadIconUrl(&out->icon_url) ||
      !data.ReadFederationOrigin(&out->federation_origin))
    return false;

  out->skip_zero_click = data.skip_zero_click();

  out->was_parsed_using_autofill_predictions =
      data.was_parsed_using_autofill_predictions();
  out->is_public_suffix_match = data.is_public_suffix_match();
  out->is_affiliation_based_match = data.is_affiliation_based_match();
  out->only_for_fallback_saving = data.only_for_fallback_saving();
  out->is_gaia_with_skip_save_password_form =
      data.is_gaia_with_skip_save_password_form();

  return true;
}

// static
std::vector<autofill::FormFieldData>
StructTraits<autofill::mojom::PasswordFormFieldPredictionMapDataView,
             autofill::PasswordFormFieldPredictionMap>::
    keys(const autofill::PasswordFormFieldPredictionMap& r) {
  std::vector<autofill::FormFieldData> data;
  for (const auto& i : r)
    data.push_back(i.first);
  return data;
}

// static
std::vector<autofill::PasswordFormFieldPredictionType>
StructTraits<autofill::mojom::PasswordFormFieldPredictionMapDataView,
             autofill::PasswordFormFieldPredictionMap>::
    values(const autofill::PasswordFormFieldPredictionMap& r) {
  std::vector<autofill::PasswordFormFieldPredictionType> types;
  for (const auto& i : r)
    types.push_back(i.second);
  return types;
}

// static
bool StructTraits<autofill::mojom::PasswordFormFieldPredictionMapDataView,
                  autofill::PasswordFormFieldPredictionMap>::
    Read(autofill::mojom::PasswordFormFieldPredictionMapDataView data,
         autofill::PasswordFormFieldPredictionMap* out) {
  // Combines keys vector and values vector to the map.
  std::vector<autofill::FormFieldData> keys;
  if (!data.ReadKeys(&keys))
    return false;
  std::vector<autofill::PasswordFormFieldPredictionType> values;
  if (!data.ReadValues(&values))
    return false;
  if (keys.size() != values.size())
    return false;
  out->clear();
  for (size_t i = 0; i < keys.size(); ++i)
    out->insert({keys[i], values[i]});

  return true;
}

// static
std::vector<autofill::FormData> StructTraits<
    autofill::mojom::FormsPredictionsMapDataView,
    autofill::FormsPredictionsMap>::keys(const autofill::FormsPredictionsMap&
                                             r) {
  std::vector<autofill::FormData> data;
  for (const auto& i : r)
    data.push_back(i.first);
  return data;
}

// static
std::vector<autofill::PasswordFormFieldPredictionMap> StructTraits<
    autofill::mojom::FormsPredictionsMapDataView,
    autofill::FormsPredictionsMap>::values(const autofill::FormsPredictionsMap&
                                               r) {
  std::vector<autofill::PasswordFormFieldPredictionMap> maps;
  for (const auto& i : r)
    maps.push_back(i.second);
  return maps;
}

// static
bool StructTraits<autofill::mojom::FormsPredictionsMapDataView,
                  autofill::FormsPredictionsMap>::
    Read(autofill::mojom::FormsPredictionsMapDataView data,
         autofill::FormsPredictionsMap* out) {
  // Combines keys vector and values vector to the map.
  std::vector<autofill::FormData> keys;
  if (!data.ReadKeys(&keys))
    return false;
  std::vector<autofill::PasswordFormFieldPredictionMap> values;
  if (!data.ReadValues(&values))
    return false;
  if (keys.size() != values.size())
    return false;
  out->clear();
  for (size_t i = 0; i < keys.size(); ++i)
    out->insert({keys[i], values[i]});

  return true;
}

// static
bool StructTraits<autofill::mojom::ValueElementPairDataView,
                  autofill::ValueElementPair>::
    Read(autofill::mojom::ValueElementPairDataView data,
         autofill::ValueElementPair* out) {
  if (!data.ReadValue(&out->first) || !data.ReadFieldName(&out->second))
    return false;

  return true;
}

}  // namespace mojo
