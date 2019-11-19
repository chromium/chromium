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
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
// The timeout for any JavaScript call in this file.
const int64_t kJavaScriptExecutionTimeoutInSeconds = 5;
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
  // Convert JSON string to JSON object |JSONValue|.
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> json_value(
      base::JSONReader::ReadAndReturnErrorDeprecated(
          base::SysNSStringToUTF8(json_string), base::JSON_PARSE_RFC,
          &error_code, &error_message));
  if (error_code)
    return nullptr;

  return json_value;
}

bool ExtractFormsData(NSString* forms_json,
                      bool filtered,
                      const base::string16& form_name,
                      const GURL& main_frame_url,
                      const GURL& frame_origin,
                      std::vector<FormData>* forms_data) {
  DCHECK(forms_data);
  std::unique_ptr<base::Value> forms_value = ParseJson(forms_json);
  if (!forms_value)
    return false;

  // Returned data should be a list of forms.
  const base::ListValue* forms_list = nullptr;
  if (!forms_value->GetAsList(&forms_list))
    return false;

  // Iterate through all the extracted forms and copy the data from JSON into
  // AutofillManager structures.
  for (const auto& form_dict : *forms_list) {
    autofill::FormData form;
    if (ExtractFormData(form_dict, filtered, form_name, main_frame_url,
                        frame_origin, &form))
      forms_data->push_back(std::move(form));
  }
  return true;
}

bool ExtractFormData(const base::Value& form_value,
                     bool filtered,
                     const base::string16& form_name,
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
  base::string16 origin;
  if (!form_dictionary->GetString("origin", &origin))
    return false;

  // Use GURL object to verify origin of host frame URL.
  form_data->url = GURL(origin);
  if (form_data->url.GetOrigin() != form_frame_origin)
    return false;

  // main_frame_origin is used for logging UKM.
  form_data->main_frame_origin = url::Origin::Create(main_frame_url);

  // Action is optional.
  base::string16 action;
  form_dictionary->GetString("action", &action);
  form_data->action = GURL(action);

  // Optional fields.
  form_dictionary->GetString("name_attribute", &form_data->name_attribute);
  form_dictionary->GetString("id_attribute", &form_data->id_attribute);
  form_dictionary->GetBoolean("is_form_tag", &form_data->is_form_tag);
  form_dictionary->GetBoolean("is_formless_checkout",
                              &form_data->is_formless_checkout);

  // Field list (mandatory) is extracted.
  const base::ListValue* fields_list = nullptr;
  if (!form_dictionary->GetList("fields", &fields_list))
    return false;
  for (const auto& field_dict : *fields_list) {
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

  // Optional fields.
  field.GetString("name_attribute", &field_data->name_attribute);
  field.GetString("id_attribute", &field_data->id_attribute);
  field.GetString("label", &field_data->label);
  field.GetString("value", &field_data->value);
  field.GetString("autocomplete_attribute",
                  &field_data->autocomplete_attribute);
  field.GetBoolean("is_autofilled", &field_data->is_autofilled);

  int max_length = 0;
  if (field.GetInteger("max_length", &max_length))
    field_data->max_length = max_length;

  // TODO(crbug.com/427614): Extract |is_checked|.
  bool is_checkable = false;
  field.GetBoolean("is_checkable", &is_checkable);
  autofill::SetCheckStatus(field_data, is_checkable, false);

  field.GetBoolean("is_focusable", &field_data->is_focusable);
  field.GetBoolean("should_autocomplete", &field_data->should_autocomplete);

  // RoleAttribute::kOther is the default value. The only other value as of this
  // writing is RoleAttribute::kPresentation.
  int role = 0;
  if (field.GetInteger("role", &role) &&
      role == static_cast<int>(FormFieldData::RoleAttribute::kPresentation)) {
    field_data->role = FormFieldData::RoleAttribute::kPresentation;
  }

  // TODO(crbug.com/427614): Extract |text_direction|.

  // Load option values where present.
  const base::ListValue* option_values = nullptr;
  if (field.GetList("option_values", &option_values)) {
    for (const auto& optionValue : *option_values) {
      base::string16 value;
      if (optionValue.GetAsString(&value))
        field_data->option_values.push_back(std::move(value));
    }
  }

  // Load option contents where present.
  const base::ListValue* option_contents = nullptr;
  if (field.GetList("option_contents", &option_contents)) {
    for (const auto& option_content : *option_contents) {
      base::string16 content;
      if (option_content.GetAsString(&content))
        field_data->option_contents.push_back(std::move(content));
    }
  }

  return field_data->option_values.size() == field_data->option_contents.size();
}

void ExecuteJavaScriptFunction(const std::string& name,
                               const std::vector<base::Value>& parameters,
                               web::WebFrame* frame,
                               CRWJSInjectionReceiver* js_injection_receiver,
                               base::OnceCallback<void(NSString*)> callback) {
  __block base::OnceCallback<void(NSString*)> cb = std::move(callback);

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
          NSString* result = nil;
          if (res && res->is_string()) {
            result = base::SysUTF8ToNSString(res->GetString());
          }
          std::move(cb).Run(result);
        }),
        base::TimeDelta::FromSeconds(kJavaScriptExecutionTimeoutInSeconds));
    if (!called) {
      std::move(cb).Run(nil);
    }
  } else {
    frame->CallJavaScriptFunction(name, parameters);
  }
}

}  // namespace autofill
