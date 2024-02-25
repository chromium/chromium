// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_fill_data.h"

#include <tuple>
#include <unordered_set>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

PasswordAndMetadata::PasswordAndMetadata() = default;
PasswordAndMetadata::PasswordAndMetadata(const PasswordAndMetadata&) = default;
PasswordAndMetadata::PasswordAndMetadata(PasswordAndMetadata&&) = default;
PasswordAndMetadata& PasswordAndMetadata::operator=(
    const PasswordAndMetadata&) = default;
PasswordAndMetadata& PasswordAndMetadata::operator=(PasswordAndMetadata&&) =
    default;
PasswordAndMetadata::~PasswordAndMetadata() = default;

PasswordSuggestionRequest::PasswordSuggestionRequest(
    FieldRendererId element_id,
    const FormData& form_data,
    AutofillSuggestionTriggerSource trigger_source,
    uint64_t username_field_index,
    uint64_t password_field_index,
    base::i18n::TextDirection text_direction,
    const std::u16string& typed_username,
    bool show_webauthn_credentials,
    const gfx::RectF& bounds)
    : element_id(element_id),
      form_data(form_data),
      trigger_source(trigger_source),
      username_field_index(username_field_index),
      password_field_index(password_field_index),
      text_direction(text_direction),
      typed_username(typed_username),
      show_webauthn_credentials(show_webauthn_credentials),
      bounds(bounds) {}

PasswordSuggestionRequest::PasswordSuggestionRequest() = default;
PasswordSuggestionRequest::PasswordSuggestionRequest(
    const PasswordSuggestionRequest&) = default;
PasswordSuggestionRequest& PasswordSuggestionRequest::operator=(
    const PasswordSuggestionRequest&) = default;
PasswordSuggestionRequest::PasswordSuggestionRequest(
    PasswordSuggestionRequest&&) = default;
PasswordSuggestionRequest& PasswordSuggestionRequest::operator=(
    PasswordSuggestionRequest&&) = default;
PasswordSuggestionRequest::~PasswordSuggestionRequest() = default;

PasswordFormFillData::PasswordFormFillData() = default;
PasswordFormFillData::PasswordFormFillData(const PasswordFormFillData&) =
    default;
PasswordFormFillData& PasswordFormFillData::operator=(
    const PasswordFormFillData&) = default;
PasswordFormFillData::PasswordFormFillData(PasswordFormFillData&&) = default;
PasswordFormFillData& PasswordFormFillData::operator=(PasswordFormFillData&&) =
    default;
PasswordFormFillData::~PasswordFormFillData() = default;

PasswordFormFillData MaybeClearPasswordValues(
    const PasswordFormFillData& data) {
  // In case when there is a username on a page (for example in a hidden field),
  // credentials from |additional_logins| could be used for filling on load. So
  // in case of filling on load nor |password_field| nor |additional_logins|
  // can't be cleared
  bool is_fallback = data.password_element_renderer_id.is_null();
  if (!data.wait_for_username && !is_fallback)
    return data;
  PasswordFormFillData result(data);
  result.preferred_login.password_value.clear();
  for (auto& credentials : result.additional_logins)
    credentials.password_value.clear();
  return result;
}

}  // namespace autofill
