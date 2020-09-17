// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/js_password_manager.h"

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/ios/browser/autofill_util.h"
#include "components/password_manager/ios/account_select_fill_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::CreateBoolCallback;
using autofill::CreateStringCallback;
using autofill::FormRendererId;
using autofill::FieldRendererId;
using autofill::kNotSetRendererID;
using base::SysNSStringToUTF8;

namespace password_manager {

std::unique_ptr<base::Value> SerializeFillData(
    const GURL& origin,
    FormRendererId form_renderer_id,
    FieldRendererId username_element,
    const base::string16& username_value,
    FieldRendererId password_element,
    const base::string16& password_value) {
  auto rootDict = std::make_unique<base::DictionaryValue>();
  rootDict->SetString("origin", origin.spec());
  rootDict->SetInteger("unique_renderer_id", form_renderer_id.value());

  auto fieldList = std::make_unique<base::ListValue>();

  auto usernameField = std::make_unique<base::DictionaryValue>();
  usernameField->SetInteger("unique_renderer_id", username_element
                                                      ? username_element.value()
                                                      : kNotSetRendererID);
  usernameField->SetString("value", username_value);
  fieldList->Append(std::move(usernameField));

  auto passwordField = std::make_unique<base::DictionaryValue>();
  passwordField->SetInteger("unique_renderer_id", password_element.value());
  passwordField->SetString("value", password_value);
  fieldList->Append(std::move(passwordField));

  rootDict->Set("fields", std::move(fieldList));

  return rootDict;
}

std::unique_ptr<base::Value> SerializePasswordFormFillData(
    const autofill::PasswordFormFillData& formData) {
  return SerializeFillData(formData.url, formData.form_renderer_id,
                           formData.username_field.unique_renderer_id,
                           formData.username_field.value,
                           formData.password_field.unique_renderer_id,
                           formData.password_field.value);
}

std::unique_ptr<base::Value> SerializeFillData(
    const password_manager::FillData& fillData) {
  return SerializeFillData(
      fillData.origin, fillData.form_id, fillData.username_element_id,
      fillData.username_value, fillData.password_element_id,
      fillData.password_value);
}

}  // namespace password_manager

@implementation JsPasswordManager

- (void)findPasswordFormsInFrame:(web::WebFrame*)frame
               completionHandler:(void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  std::vector<base::Value> parameters;
  autofill::ExecuteJavaScriptFunction("passwords.findPasswordForms", parameters,
                                      frame,
                                      CreateStringCallback(completionHandler));
}

- (void)extractForm:(FormRendererId)formIdentifier
              inFrame:(web::WebFrame*)frame
    completionHandler:(void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  std::vector<base::Value> parameters;
  parameters.emplace_back(static_cast<int>(formIdentifier.value()));
  autofill::ExecuteJavaScriptFunction("passwords.getPasswordFormDataAsString",
                                      parameters, frame,
                                      CreateStringCallback(completionHandler));
}

- (void)fillPasswordForm:(std::unique_ptr<base::Value>)form
                 inFrame:(web::WebFrame*)frame
            withUsername:(std::string)username
                password:(std::string)password
       completionHandler:(void (^)(BOOL))completionHandler {
  DCHECK(completionHandler);
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*form));
  parameters.emplace_back(std::move(username));
  parameters.emplace_back(std::move(password));
  autofill::ExecuteJavaScriptFunction("passwords.fillPasswordForm", parameters,
                                      frame,
                                      CreateBoolCallback(completionHandler));
}

- (void)fillPasswordForm:(FormRendererId)formIdentifier
                      inFrame:(web::WebFrame*)frame
        newPasswordIdentifier:(FieldRendererId)newPasswordIdentifier
    confirmPasswordIdentifier:(FieldRendererId)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(void (^)(BOOL))completionHandler {
  DCHECK(completionHandler);
  std::vector<base::Value> parameters;
  parameters.emplace_back(static_cast<int>(formIdentifier.value()));
  parameters.emplace_back(static_cast<int>(newPasswordIdentifier.value()));
  parameters.emplace_back(static_cast<int>(confirmPasswordIdentifier.value()));
  parameters.push_back(base::Value(SysNSStringToUTF8(generatedPassword)));
  autofill::ExecuteJavaScriptFunction(
      "passwords.fillPasswordFormWithGeneratedPassword", parameters, frame,
      CreateBoolCallback(completionHandler));
}

- (void)setUpForUniqueIDsWithInitialState:(uint32_t)nextAvailableID
                                  inFrame:(web::WebFrame*)frame {
  std::vector<base::Value> parameters;
  parameters.emplace_back(static_cast<int>(nextAvailableID));
  autofill::ExecuteJavaScriptFunction("fill.setUpForUniqueIDs", parameters,
                                      frame,
                                      autofill::JavaScriptResultCallback());
}

@end
