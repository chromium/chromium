// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_util.h"

#import <utility>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/types/cxx23_to_underlying.h"
#import "base/values.h"
#import "components/autofill/core/browser/autofill_field.h"
#import "components/autofill/core/common/autocomplete_parsing_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_util.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/signatures.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"
#import "url/origin.h"

using autofill::FormControlType;
using base::NumberToString;
using base::StringToUint;

namespace {
// The timeout for any JavaScript call in this file.
const int64_t kJavaScriptExecutionTimeoutInSeconds = 5;

// Runs |callback| with the NSString value of |res|.
// |callback| must be non-null.
void ConvertValueToNSString(base::OnceCallback<void(NSString*)> callback,
                            const base::Value* res) {
  DCHECK(!callback.is_null());

  NSString* result = nil;
  if (res && res->is_string()) {
    result = base::SysUTF8ToNSString(res->GetString());
  }
  std::move(callback).Run(result);
}

// Runs |callback| with the BOOL value of |res|. |callback| must be non-null.
void ConvertValueToBool(base::OnceCallback<void(BOOL)> callback,
                        const base::Value* res) {
  DCHECK(!callback.is_null());

  BOOL result = NO;
  if (res && res->is_bool()) {
    result = res->GetBool();
  }
  std::move(callback).Run(result);
}

}  // namespace

namespace autofill {

bool IsContextSecureForWebState(web::WebState* web_state) {
  // This implementation differs slightly from other platforms. Other platforms'
  // implementations check for the presence of active mixed content, but because
  // the iOS web view blocks active mixed content without an option to run it,
  // there is no need to check for active mixed content here.
  web::NavigationManager* manager = web_state->GetNavigationManager();
  const web::NavigationItem* nav_item = manager->GetLastCommittedItem();
  if (!nav_item) {
    return false;
  }

  const web::SSLStatus& ssl = nav_item->GetSSL();
  return nav_item->GetURL().SchemeIsCryptographic() && ssl.certificate &&
         !net::IsCertStatusError(ssl.cert_status);
}

std::unique_ptr<base::Value> ParseJson(NSString* json_string) {
  std::optional<base::Value> json_value =
      base::JSONReader::Read(base::SysNSStringToUTF8(json_string));
  if (!json_value) {
    return nullptr;
  }
  return base::Value::ToUniquePtrValue(std::move(*json_value));
}

std::optional<base::UnguessableToken> DeserializeJavaScriptFrameId(
    const std::string& frame_id) {
  // A valid ID is 128 bits, or 32 hex digits.
  if (frame_id.length() != 32) {
    return std::nullopt;
  }

  // Break string into first and last 16 hex digits.
  std::string high_hex = frame_id.substr(0, 16);
  std::string low_hex = frame_id.substr(16);

  uint64_t high, low;
  if (!base::HexStringToUInt64(high_hex, &high) ||
      !base::HexStringToUInt64(low_hex, &low)) {
    return std::nullopt;
  }

  return base::UnguessableToken::Deserialize(high, low);
}

bool ExtractFormsData(NSString* forms_json,
                      bool filtered,
                      const std::u16string& form_name,
                      const GURL& main_frame_url,
                      const GURL& frame_origin,
                      const FieldDataManager& field_data_manager,
                      const std::string& frame_id,
                      std::vector<FormData>* forms_data) {
  DCHECK(forms_data);
  std::unique_ptr<base::Value> forms_value = ParseJson(forms_json);
  if (!forms_value) {
    return false;
  }

  // Returned data should be a list of forms.
  if (!forms_value->is_list()) {
    return false;
  }

  // Iterate through all the extracted forms and copy the data from JSON into
  // BrowserAutofillManager structures.
  for (const auto& form_value : forms_value->GetList()) {
    const auto* form_dict = form_value.GetIfDict();
    if (!form_dict) {
      continue;
    }
    autofill::FormData form;
    if (ExtractFormData(*form_dict, filtered, form_name, main_frame_url,
                        frame_origin, field_data_manager, frame_id, &form)) {
      forms_data->push_back(std::move(form));
    }
  }
  return true;
}

bool ExtractFormData(const base::Value::Dict& form,
                     bool filtered,
                     const std::u16string& form_name,
                     const GURL& main_frame_url,
                     const GURL& form_frame_origin,
                     const FieldDataManager& field_data_manager,
                     const std::string& frame_id,
                     autofill::FormData* form_data) {
  DCHECK(form_data);
  // Form data is copied into a FormData object field-by-field.
  const std::string* name = form.FindString("name");
  if (!name) {
    return false;
  }
  form_data->set_name(base::UTF8ToUTF16(*name));
  if (filtered && form_name != form_data->name()) {
    return false;
  }

  // Origin is mandatory.
  const std::string* origin_ptr = form.FindString("origin");
  if (!origin_ptr) {
    return false;
  }
  std::u16string origin = base::UTF8ToUTF16(*origin_ptr);

  // Use GURL object to verify origin of host frame URL.
  form_data->set_url(GURL(origin));
  if (form_data->url().DeprecatedGetOriginAsURL() != form_frame_origin) {
    return false;
  }

  bool include_frame_metadata = base::FeatureList::IsEnabled(
      autofill::features::kAutofillAcrossIframesIos);

  const url::Origin frame_origin_object =
      include_frame_metadata ? url::Origin::Create(form_frame_origin)
                             : url::Origin();

  // Frame ID of the frame containing this form is mandatory.
  const std::string* host_frame_param = form.FindString("host_frame");
  if (!host_frame_param) {
    return false;
  }

  std::optional<base::UnguessableToken> host_frame =
      DeserializeJavaScriptFrameId(*host_frame_param);
  if (!host_frame) {
    return false;
  }

  form_data->set_host_frame(LocalFrameToken(*host_frame));

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAcrossIframesIos) &&
      *host_frame_param != frame_id) {
    // Invalidate parsing when the the frame for which extraction was done
    // doesn't correspond to the frame where extraction actually happened.
    // This is to prevent associating the form data with the wrong frame.
    return false;
  }

  // main_frame_origin is used for logging UKM.
  form_data->set_main_frame_origin(url::Origin::Create(main_frame_url));

  const std::string* renderer_id = form.FindString("renderer_id");
  if (renderer_id && !renderer_id->empty()) {
    FormRendererId form_renderer_id;
    StringToUint(*renderer_id, &form_renderer_id.value());
    form_data->set_renderer_id(form_renderer_id);
  } else {
    form_data->set_renderer_id(FormRendererId());
  }

  // Action is optional.
  std::u16string action;
  if (const std::string* action_ptr = form.FindString("action")) {
    action = base::UTF8ToUTF16(*action_ptr);
  }
  form_data->set_action(GURL(action));

  // Optional fields.
  if (const std::string* name_attribute = form.FindString("name_attribute")) {
    form_data->set_name_attribute(base::UTF8ToUTF16(*name_attribute));
  }
  if (const std::string* id_attribute = form.FindString("id_attribute")) {
    form_data->set_id_attribute(base::UTF8ToUTF16(*id_attribute));
  }

  if (include_frame_metadata) {
    // Child frame tokens, optional.
    if (const base::Value::List* child_frames_list =
            form.FindList("child_frames")) {
      std::vector<autofill::FrameTokenWithPredecessor> child_frames;
      for (const auto& frame_dict : *child_frames_list) {
        autofill::FrameTokenWithPredecessor token;
        if (frame_dict.is_dict() &&
            ExtractRemoteFrameToken(frame_dict.GetDict(), &token)) {
          child_frames.push_back(std::move(token));
        }
      }
      form_data->set_child_frames(std::move(child_frames));
    }
  }

  // Field list (mandatory) is extracted.
  const base::Value::List* fields_list = form.FindList("fields");
  if (!fields_list) {
    return false;
  }
  std::vector<FormFieldData> fields;
  fields.reserve(fields_list->size());
  for (const auto& field_dict : *fields_list) {
    autofill::FormFieldData field_data;
    if (field_dict.is_dict() &&
        ExtractFormFieldData(field_dict.GetDict(), field_data_manager,
                             &field_data)) {
      // Some data is extracted at the form level, but also appears at the
      // field level. Reuse the extracted values.
      field_data.set_host_form_id(form_data->renderer_id());
      field_data.set_host_frame(form_data->host_frame());
      if (include_frame_metadata) {
        field_data.set_origin(frame_origin_object);
      }

      fields.push_back(std::move(field_data));
    } else {
      return false;
    }
  }
  form_data->set_fields(std::move(fields));

  if (include_frame_metadata) {
    FormSignature form_signature = CalculateFormSignature(*form_data);
    std::vector<FormFieldData> form_fields = form_data->ExtractFields();
    for (FormFieldData& field : form_fields) {
      field.set_host_form_signature(form_signature);
    }
    form_data->set_fields(std::move(form_fields));
  }
  return true;
}

bool ExtractFormFieldData(const base::Value::Dict& field,
                          const FieldDataManager& field_data_manager,
                          autofill::FormFieldData* field_data) {
  const std::string* name;
  const std::string* form_control_type;
  if (!(name = field.FindString("name")) ||
      !(form_control_type = field.FindString("form_control_type"))) {
    return false;
  }

  field_data->set_name(base::UTF8ToUTF16(*name));
  field_data->set_form_control_type(
      autofill::StringToFormControlTypeDiscouraged(*form_control_type,
                                                   /*fallback=*/std::nullopt));

  const std::string* renderer_id = field.FindString("renderer_id");
  if (renderer_id && !renderer_id->empty()) {
    FieldRendererId field_renderer_id;
    StringToUint(*renderer_id, &field_renderer_id.value());
    field_data->set_renderer_id(field_renderer_id);
  } else {
    field_data->set_renderer_id(FieldRendererId());
  }

  // Optional fields.
  if (const std::string* name_attribute = field.FindString("name_attribute")) {
    field_data->set_name_attribute(base::UTF8ToUTF16(*name_attribute));
  }
  if (const std::string* id_attribute = field.FindString("id_attribute")) {
    field_data->set_id_attribute(base::UTF8ToUTF16(*id_attribute));
  }
  if (const std::string* label = field.FindString("label")) {
    field_data->set_label(base::UTF8ToUTF16(*label));
  }
  if (const std::string* value = field.FindString("value")) {
    field_data->set_value(base::UTF8ToUTF16(*value));
  }
  field_data->set_is_autofilled(
      field.FindBool("is_autofilled").value_or(field_data->is_autofilled()));
  field_data->set_is_user_edited(
      field.FindBool("is_user_edited").value_or(field_data->is_user_edited()));

  if (const std::string* autocomplete_attribute =
          field.FindString("autocomplete_attribute")) {
    field_data->set_autocomplete_attribute(*autocomplete_attribute);
  }
  if (std::optional<int> max_length = field.FindInt("max_length")) {
    field_data->set_max_length(*max_length);
  }
  field_data->set_parsed_autocomplete(
      ParseAutocompleteAttribute(field_data->autocomplete_attribute()));

  // TODO(crbug.com/40391162): Extract |is_checked|.
  bool is_checkable = field.FindBool("is_checkable").value_or(false);
  autofill::SetCheckStatus(field_data, is_checkable, false);

  field_data->set_is_focusable(
      field.FindBool("is_focusable").value_or(field_data->is_focusable()));
  field_data->set_should_autocomplete(
      field.FindBool("should_autocomplete")
          .value_or(field_data->should_autocomplete()));

  if (const std::string* placeholder_attribute =
          field.FindString("placeholder_attribute")) {
    field_data->set_placeholder(base::UTF8ToUTF16(*placeholder_attribute));
  }

  if (const std::string* aria_label = field.FindString("aria_label")) {
    field_data->set_aria_label(base::UTF8ToUTF16(*aria_label));
  }
  if (const std::string* aria_description =
          field.FindString("aria_description")) {
    field_data->set_aria_description(base::UTF8ToUTF16(*aria_description));
  }

  // RoleAttribute::kOther is the default value. The only other value as of this
  // writing is RoleAttribute::kPresentation.
  std::optional<int> role = field.FindInt("role");
  if (role &&
      *role == static_cast<int>(FormFieldData::RoleAttribute::kPresentation)) {
    field_data->set_role(FormFieldData::RoleAttribute::kPresentation);
  }

  // TODO(crbug.com/40391162): Extract |text_direction|.

  // Load option values where present.
  const base::Value::List* option_values = field.FindList("option_values");
  const base::Value::List* option_texts = field.FindList("option_texts");
  if (option_values && option_texts) {
    if (option_values->size() != option_texts->size()) {
      return false;
    }
    std::vector<SelectOption> options;
    auto value_it = option_values->begin();
    auto text_it = option_texts->begin();
    while (value_it != option_values->end() && text_it != option_texts->end()) {
      if (value_it->is_string() && text_it->is_string()) {
        options.push_back({.value = base::UTF8ToUTF16(value_it->GetString()),
                           .text = base::UTF8ToUTF16(text_it->GetString())});
      }
      ++value_it;
      ++text_it;
    }
    field_data->set_options(std::move(options));
  }

  // Fill user input and properties mask.
  if (field_data_manager.HasFieldData(field_data->renderer_id())) {
    field_data->set_user_input(
        field_data_manager.GetUserInput(field_data->renderer_id()));
    field_data->set_properties_mask(
        field_data_manager.GetFieldPropertiesMask(field_data->renderer_id()));
  }

  return true;
}

bool ExtractRemoteFrameToken(
    const base::Value::Dict& frame_data,
    FrameTokenWithPredecessor* token_with_predecessor) {
  const std::string* frame_id = frame_data.FindString("token");
  if (!frame_id) {
    return false;
  }

  std::optional<base::UnguessableToken> token =
      DeserializeJavaScriptFrameId(*frame_id);
  if (!token) {
    return false;
  }

  const std::optional<double> predecessor =
      frame_data.FindDouble("predecessor");
  if (!predecessor) {
    return false;
  }

  token_with_predecessor->token = RemoteFrameToken(*token);
  token_with_predecessor->predecessor = *predecessor;
  return true;
}

JavaScriptResultCallback CreateStringCallback(
    void (^completionHandler)(NSString*)) {
  return CreateStringCallback(base::BindOnce(completionHandler));
}

JavaScriptResultCallback CreateStringCallback(
    base::OnceCallback<void(NSString*)> callback) {
  return base::BindOnce(&ConvertValueToNSString, std::move(callback));
}

JavaScriptResultCallback CreateBoolCallback(void (^completionHandler)(BOOL)) {
  return CreateBoolCallback(base::BindOnce(completionHandler));
}

JavaScriptResultCallback CreateBoolCallback(
    base::OnceCallback<void(BOOL)> callback) {
  return base::BindOnce(&ConvertValueToBool, std::move(callback));
}

void ExecuteJavaScriptFunction(const std::string& name,
                               const base::Value::List& parameters,
                               web::WebFrame* frame,
                               JavaScriptResultCallback callback) {
  __block JavaScriptResultCallback cb = std::move(callback);

  if (!frame) {
    if (!cb.is_null()) {
      std::move(cb).Run(nil);
    }
    return;
  }
  if (!cb.is_null()) {
    bool called = frame->CallJavaScriptFunction(
        name, parameters, base::BindOnce(^(const base::Value* res) {
          std::move(cb).Run(res);
        }),
        base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
    if (!called) {
      std::move(cb).Run(nil);
    }
  } else {
    frame->CallJavaScriptFunction(name, parameters);
  }
}

bool ExtractFillingResults(
    NSString* json_string,
    std::map<uint32_t, std::u16string>* filling_results) {
  DCHECK(filling_results);
  std::unique_ptr<base::Value> ids_value = ParseJson(json_string);
  if (!ids_value || !ids_value->is_dict()) {
    return false;
  }

  for (const auto result : ids_value->GetDict()) {
    std::string id_string = result.first;
    uint32_t id_num = 0;
    StringToUint(id_string, &id_num);
    (*filling_results)[id_num] = base::UTF8ToUTF16(result.second.GetString());
  }
  return true;
}

web::WebFramesManager* GetWebFramesManagerForAutofill(
    web::WebState* web_state) {
  CHECK(web_state);
  return web_state->GetWebFramesManager(
      ContentWorldForAutofillJavascriptFeatures());
}

}  // namespace autofill
