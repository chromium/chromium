// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_UTIL_H_

#include <vector>

#import "ios/web/public/js_messaging/web_frame.h"

@class CRWJSInjectionReceiver;
class GURL;

namespace base {
class DictionaryValue;
}
namespace web {
class WebState;
}

namespace autofill {

struct FormData;
struct FormFieldData;

// Checks if current context is secure from an autofill standpoint.
bool IsContextSecureForWebState(web::WebState* web_state);

// Tries to parse a JSON string into base::Value. Returns nullptr if parsing is
// unsuccessful.
std::unique_ptr<base::Value> ParseJson(NSString* json_string);

// Processes the JSON form data extracted from the page into the format expected
// by AutofillManager and fills it in |forms_data|.
// |forms_data| cannot be nil.
// |filtered| and |form_name| limit the field that will be returned in
// |forms_data|.
// Returns a bool indicating the success value and the vector of form data.
bool ExtractFormsData(NSString* form_json,
                      bool filtered,
                      const base::string16& form_name,
                      const GURL& main_frame_url,
                      const GURL& frame_origin,
                      std::vector<FormData>* forms_data);

// Converts |form| into |form_data|.
// Returns false if a form can not be extracted.
// Returns false if |filtered| == true and |form["name"]| != |formName|.
// Returns false if |form["origin"]| != |form_frame_origin|.
// Returns true if the conversion succeeds.
bool ExtractFormData(const base::Value& form,
                     bool filtered,
                     const base::string16& form_name,
                     const GURL& main_frame_url,
                     const GURL& form_frame_origin,
                     FormData* form_data);

// Extracts a single form field from the JSON dictionary into a FormFieldData
// object.
// Returns false if the field could not be extracted.
bool ExtractFormFieldData(const base::DictionaryValue& field,
                          FormFieldData* field_data);

// Executes the JavaScript function with the given name and argument.
// If |callback| is not null, it will be called when the result of the
// command is received, or immediately if the command cannot be executed.
void ExecuteJavaScriptFunction(const std::string& name,
                               const std::vector<base::Value>& parameters,
                               web::WebFrame* frame,
                               CRWJSInjectionReceiver* js_injection_receiver,
                               base::OnceCallback<void(NSString*)> callback);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_UTIL_H_
