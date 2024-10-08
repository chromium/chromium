// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_UTIL_H_

#import <optional>
#import <set>

#import "base/unguessable_token.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"

class GURL;

namespace web {
class WebFramesManager;
class WebState;
}

namespace autofill {

class FormData;
class FormFieldData;
class FieldDataManager;
struct FrameTokenWithPredecessor;

// Checks if current context is secure from an autofill standpoint.
bool IsContextSecureForWebState(web::WebState* web_state);

// Tries to parse a JSON string into base::Value. Returns nullptr if parsing is
// unsuccessful.
std::unique_ptr<base::Value> ParseJson(NSString* json_string);

// Local and remote frame IDs generated in JavaScript are equivalent to
// base::UnguessableToken (128 bits, cryptographically random). Returns a
// base::UnguessableToken equivalent to the given JS-generated ID.
std::optional<base::UnguessableToken> DeserializeJavaScriptFrameId(
    const std::string& frame_id);

// Processes the JSON form data extracted from the page into the format expected
// by BrowserAutofillManager and fills it in |forms_data|.
// |forms_data| cannot be nil.
// |filtered| and |form_name| limit the field that will be returned in
// |forms_data|.
// Returns a bool indicating the success value and the vector of form data.
bool ExtractFormsData(NSString* form_json,
                      bool filtered,
                      const std::u16string& form_name,
                      const GURL& main_frame_url,
                      const GURL& frame_origin,
                      const FieldDataManager& field_data_manager,
                      const std::string& frame_id,
                      std::vector<FormData>* forms_data);

// Converts |form| into |form_data|.
// Returns false if a form can not be extracted.
// Returns false if |filtered| == true and |form["name"]| !=
// |formName|. Returns false if |form["origin"]| !=
// |form_frame_origin|. Returns true if the conversion succeeds.
bool ExtractFormData(const base::Value::Dict& form,
                     bool filtered,
                     const std::u16string& form_name,
                     const GURL& main_frame_url,
                     const GURL& form_frame_origin,
                     const FieldDataManager& field_data_manager,
                     const std::string& frame_id,
                     FormData* form_data);

// Extracts a single form field from the JSON dictionary into a FormFieldData
// object.
// Returns false if the field could not be extracted.
bool ExtractFormFieldData(const base::Value::Dict& field,
                          const FieldDataManager& field_data_manager,
                          FormFieldData* field_data);

// Extracts a single child frame's data from the JSON dictionary into a
// FrameTokenWithPredecessor object. Returns false if the data could not be
// extracted.
bool ExtractRemoteFrameToken(const base::Value::Dict& frame_data,
                             FrameTokenWithPredecessor* token_with_predecessor);

typedef base::OnceCallback<void(const base::Value*)> JavaScriptResultCallback;

// Creates a callback for a string JS function return type.
JavaScriptResultCallback CreateStringCallback(
    void (^completionHandler)(NSString*));
JavaScriptResultCallback CreateStringCallback(
    base::OnceCallback<void(NSString*)> callback);

// Creates a callback for a bool JS function return type.
JavaScriptResultCallback CreateBoolCallback(void (^completionHandler)(BOOL));
JavaScriptResultCallback CreateBoolCallback(base::OnceCallback<void(BOOL)>);

// Executes the JavaScript function with the given name and argument.
// If |callback| is not null, it will be called when the result of the
// command is received, or immediately if the command cannot be executed.
void ExecuteJavaScriptFunction(const std::string& name,
                               const base::Value::List& parameters,
                               web::WebFrame* frame,
                               JavaScriptResultCallback callback);

// Extracts a vector of 32 bits numeric renderer IDs from the JS returned json
// string.
// - IDType: Identifier type must be constructable from uint32_t.
template <typename IDType>
std::optional<std::set<IDType>> ExtractIDs(NSString* json_string) {
  std::unique_ptr<base::Value> ids_value = ParseJson(json_string);

  if (!ids_value || !ids_value->is_list()) {
    return std::nullopt;
  }

  std::set<IDType> ids;

  for (const auto& unique_id : ids_value->GetList()) {
    if (!unique_id.is_string()) {
      return std::nullopt;
    }
    uint32_t id_num = 0;
    if (!base::StringToUint(unique_id.GetString(), &id_num)) {
      return std::nullopt;
    }
    ids.insert(IDType(id_num));
  }

  return ids;
}

// Extracts a map of filled renderer IDs and values from the JS returned json
// string.
bool ExtractFillingResults(NSString* json_string,
                           std::map<uint32_t, std::u16string>* filling_results);

// Returns the WebFramesManager that manages the frame space in which Autofill
// works.
web::WebFramesManager* GetWebFramesManagerForAutofill(web::WebState* web_state);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_UTIL_H_
