// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_util.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"
#include "url/origin.h"

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
}

namespace autofill {

bool IsContextSecureForWebState(web::WebState* web_state) {
  // This implementation differs slightly from other platforms. Other platforms'
  // implementations check for the presence of active mixed content, but because
  // the iOS web view blocks active mixed content without an option to run it,
  // there is no need to check for active mixed content here.
  web::NavigationManager* manager = web_state->GetNavigationManager();
  const web::NavigationItem* nav_item = manager->GetLastCommittedItem();
  if (!nav_item)
    return false;

  const web::SSLStatus& ssl = nav_item->GetSSL();
  return nav_item->GetURL().SchemeIsCryptographic() && ssl.certificate &&
         !net::IsCertStatusError(ssl.cert_status);
}

std::unique_ptr<base::Value> ParseJson(NSString* json_string) {
  absl::optional<base::Value> json_value =
      base::JSONReader::Read(base::SysNSStringToUTF8(json_string));
  if (!json_value)
    return nullptr;
  return base::Value::ToUniquePtrValue(std::move(*json_value));
}

bool ExtractFormsData(NSString* forms_json,
                      bool filtered,
                      const std::u16string& form_name,
                      const GURL& main_frame_url,
                      const GURL& frame_origin,
                      std::vector<FormData>* forms_data) {
  DCHECK(forms_data);
  std::unique_ptr<base::Value> forms_value = ParseJson(forms_json);
  if (!forms_value)
    return false;

  // Returned data should be a list of forms.
  if (!forms_value->is_list())
    return false;

  // Iterate through all the extracted forms and copy the data from JSON into
  // BrowserAutofillManager structures.
  for (const auto& form_dict : forms_value->GetListDeprecated()) {
    autofill::FormData form;
    if (ExtractFormData(form_dict, filtered, form_name, main_frame_url,
                        frame_origin, &form))
      forms_data->push_back(std::move(form));
  }
  return true;
}

bool ExtractFormData(const base::Value& form_value,
                     bool filtered,
                     const std::u16string& form_name,
                     const GURL& main_frame_url,
                     const GURL& form_frame_origin,
                     autofill::FormData* form_data) {
  DCHECK(form_data);
  // Each form should be a JSON dictionary.
  const base::DictionaryValue* form_dictionary = nullptr;
  if (!form_value.GetAsDictionary(&form_dictionary))
    return false;

  // Form data is copied into a FormData object field-by-field.
  if (!form_dictionary->GetString("name", &form_data->name))
    return false;
  if (filtered && form_name != form_data->name)
    return false;

  // Origin is mandatory.
  std::u16string origin;
  if (!form_dictionary->GetString("origin", &origin))
    return false;

  // Use GURL object to verify origin of host frame URL.
  form_data->url = GURL(origin);
  if (form_data->url.DeprecatedGetOriginAsURL() != form_frame_origin)
    return false;

  // main_frame_origin is used for logging UKM.
  form_data->main_frame_origin = url::Origin::Create(main_frame_url);

  std::string unique_renderer_id;
  form_dictionary->GetString("unique_renderer_id", &unique_renderer_id);
  if (!unique_renderer_id.empty()) {
    StringToUint(unique_renderer_id, &form_data->unique_renderer_id.value());
  } else {
    form_data->unique_renderer_id = FormRendererId();
  }

  // Action is optional.
  std::u16string action;
  form_dictionary->GetString("action", &action);
  form_data->action = GURL(action);

  // Optional fields.
  form_dictionary->GetString("name_attribute", &form_data->name_attribute);
  form_dictionary->GetString("id_attribute", &form_data->id_attribute);
  form_data->is_form_tag = form_dictionary->FindBoolKey("is_form_tag")
                               .value_or(form_data->is_form_tag);
  form_dictionary->GetString("frame_id", &form_data->frame_id);

  // Field list (mandatory) is extracted.
  const base::ListValue* fields_list = nullptr;
  if (!form_dictionary->GetList("fields", &fields_list))
    return false;
  for (const auto& field_dict : fields_list->GetListDeprecated()) {
    const base::DictionaryValue* field;
    autofill::FormFieldData field_data;
    if (field_dict.GetAsDictionary(&field) &&
        ExtractFormFieldData(*field, &field_data)) {
      form_data->fields.push_back(std::move(field_data));
    } else {
      return false;
    }
  }
  return true;
}

bool ExtractFormFieldData(const base::DictionaryValue& field,
                          autofill::FormFieldData* field_data) {
  if (!field.GetString("name", &field_data->name) ||
      !field.GetString("identifier", &field_data->unique_id) ||
      !field.GetString("form_control_type", &field_data->form_control_type)) {
    return false;
  }

  std::string unique_renderer_id;
  field.GetString("unique_renderer_id", &unique_renderer_id);
  if (!unique_renderer_id.empty()) {
    StringToUint(unique_renderer_id, &field_data->unique_renderer_id.value());
  } else {
    field_data->unique_renderer_id = FieldRendererId();
  }

  // Optional fields.
  field.GetString("name_attribute", &field_data->name_attribute);
  field.GetString("id_attribute", &field_data->id_attribute);
  field.GetString("label", &field_data->label);
  field.GetString("value", &field_data->value);
  field.GetString("autocomplete_attribute",
                  &field_data->autocomplete_attribute);
  field_data->is_autofilled =
      field.FindBoolKey("is_autofilled").value_or(field_data->is_autofilled);

  int max_length = 0;
  if (field.GetInteger("max_length", &max_length))
    field_data->max_length = max_length;

  // TODO(crbug.com/427614): Extract |is_checked|.
  bool is_checkable = field.FindBoolKey("is_checkable").value_or(false);
  autofill::SetCheckStatus(field_data, is_checkable, false);

  field_data->is_focusable =
      field.FindBoolKey("is_focusable").value_or(field_data->is_focusable);
  field_data->should_autocomplete =
      field.FindBoolKey("should_autocomplete")
          .value_or(field_data->should_autocomplete);

  // RoleAttribute::kOther is the default value. The only other value as of this
  // writing is RoleAttribute::kPresentation.
  int role = 0;
  if (field.GetInteger("role", &role) &&
      role == static_cast<int>(FormFieldData::RoleAttribute::kPresentation)) {
    field_data->role = FormFieldData::RoleAttribute::kPresentation;
  }

  // TODO(crbug.com/427614): Extract |text_direction|.

  // Load option values where present.
  const base::ListValue* option_values;
  const base::ListValue* option_contents;
  if (field.GetList("option_values", &option_values) &&
      field.GetList("option_contents", &option_contents)) {
    auto value_list = option_values->GetListDeprecated();
    auto content_list = option_contents->GetListDeprecated();
    if (value_list.size() != content_list.size())
      return false;
    auto value_it = value_list.begin();
    auto content_it = content_list.begin();
    while (value_it != value_list.end() && content_it != content_list.end()) {
      if (value_it->is_string() && content_it->is_string()) {
        field_data->options.push_back(
            {.value = base::UTF8ToUTF16(value_it->GetString()),
             .content = base::UTF8ToUTF16(content_it->GetString())});
      }
      ++value_it;
      ++content_it;
    }
  }

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
                               const std::vector<base::Value>& parameters,
                               web::WebFrame* frame,
                               JavaScriptResultCallback callback) {
  __block JavaScriptResultCallback cb = std::move(callback);

  if (!frame) {
    if (!cb.is_null()) {
      std::move(cb).Run(nil);
    }
    return;
  }
  DCHECK(frame->CanCallJavaScriptFunction());
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

bool ExtractIDs(NSString* json_string, std::vector<FieldRendererId>* ids) {
  DCHECK(ids);
  std::unique_ptr<base::Value> ids_value = ParseJson(json_string);
  if (!ids_value)
    return false;

  if (!ids_value->is_list())
    return false;

  for (const auto& unique_id : ids_value->GetListDeprecated()) {
    if (!unique_id.is_string())
      return false;
    uint32_t id_num = 0;
    StringToUint(unique_id.GetString(), &id_num);
    ids->push_back(FieldRendererId(id_num));
  }
  return true;
}

bool ExtractFillingResults(
    NSString* json_string,
    std::map<uint32_t, std::u16string>* filling_results) {
  DCHECK(filling_results);
  std::unique_ptr<base::Value> ids_value = ParseJson(json_string);
  if (!ids_value)
    return false;

  // Returned data should be a list of forms.
  const base::DictionaryValue* results = nullptr;
  if (!ids_value->GetAsDictionary(&results))
    return false;

  for (const auto result : results->DictItems()) {
    std::string id_string = result.first;
    uint32_t id_num = 0;
    StringToUint(id_string, &id_num);
    (*filling_results)[id_num] = base::UTF8ToUTF16(result.second.GetString());
  }
  return true;
}

}  // namespace autofill
