// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_manager_java_script_feature.h"

#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/autofill_renderer_id_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "components/password_manager/ios/password_manager_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

using autofill::CreateBoolCallback;
using autofill::CreateStringCallback;

namespace password_manager {

namespace {
constexpr char kScriptName[] = "password_controller";
constexpr char FormSubmittedHandlerName[] = "PasswordFormSubmitButtonClick";

// The timeout for any JavaScript call in this file.
constexpr int64_t kJavaScriptExecutionTimeoutInSeconds = 5;

// Converts FormRendererId to int value that can be used in Javascript methods.
int FormRendererIdToJsParameter(autofill::FormRendererId form_id) {
  return form_id.value();
}

// Converts FieldRendererId to int value that can be used in Javascript methods.
int FieldRendererIdToJsParameter(autofill::FieldRendererId field_id) {
  return field_id.value();
}

base::Value::Dict SerializeFillData(const GURL& origin,
                                    autofill::FormRendererId form_renderer_id,
                                    autofill::FieldRendererId username_element,
                                    const std::u16string& username_value,
                                    autofill::FieldRendererId password_element,
                                    const std::u16string& password_value) {
  base::Value::Dict root_dict;
  root_dict.Set("origin", origin.spec());
  root_dict.Set("renderer_id", FormRendererIdToJsParameter(form_renderer_id));

  base::Value::List fieldList;

  base::Value::Dict usernameField;
  usernameField.Set("renderer_id",
                    FieldRendererIdToJsParameter(username_element));
  usernameField.Set("value", username_value);
  fieldList.Append(std::move(usernameField));

  base::Value::Dict passwordField;
  passwordField.Set("renderer_id", static_cast<int>(password_element.value()));
  passwordField.Set("value", password_value);
  fieldList.Append(std::move(passwordField));

  root_dict.Set("fields", std::move(fieldList));

  return root_dict;
}

// Serializes |fill_data| so it can be used by the JS side of
// PasswordController. Includes both username and password data if
// |fill_username|, and only password data otherwise.
base::Value::Dict SerializeFillData(const password_manager::FillData& fill_data,
                                    BOOL fill_username) {
  return SerializeFillData(fill_data.origin, fill_data.form_id,
                           fill_username ? fill_data.username_element_id
                                         : autofill::FieldRendererId(),
                           fill_data.username_value,
                           fill_data.password_element_id,
                           fill_data.password_value);
}

}  // namespace

// static
PasswordManagerJavaScriptFeature*
PasswordManagerJavaScriptFeature::GetInstance() {
  static base::NoDestructor<PasswordManagerJavaScriptFeature> instance;
  return instance.get();
}

PasswordManagerJavaScriptFeature::PasswordManagerJavaScriptFeature()
    : web::JavaScriptFeature(
          ContentWorldForAutofillJavascriptFeatures(),
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {
              web::java_script_features::GetCommonJavaScriptFeature(),
              web::java_script_features::GetMessageJavaScriptFeature(),
              autofill::FormUtilJavaScriptFeature::GetInstance(),
              autofill::AutofillRendererIDJavaScriptFeature::GetInstance(),
          }) {}

PasswordManagerJavaScriptFeature::~PasswordManagerJavaScriptFeature() = default;

void PasswordManagerJavaScriptFeature::FindPasswordFormsInFrame(
    web::WebFrame* frame,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());
  CallJavaScriptFunction(frame, "passwords.findPasswordForms", {},
                         CreateStringCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void PasswordManagerJavaScriptFeature::ExtractForm(
    web::WebFrame* frame,
    autofill::FormRendererId form_identifier,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(!callback.is_null());
  CallJavaScriptFunction(
      frame, "passwords.getPasswordFormDataAsString",
      base::Value::List().Append(FormRendererIdToJsParameter(form_identifier)),
      CreateStringCallback(std::move(callback)),
      base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void PasswordManagerJavaScriptFeature::FillPasswordForm(
    web::WebFrame* frame,
    const password_manager::FillData& fill_data,
    BOOL fill_username,
    const std::string& username,
    const std::string& password,
    base::OnceCallback<void(const base::Value*)> callback) {
  DCHECK(!callback.is_null());

  base::Value::Dict form_value = SerializeFillData(fill_data, fill_username);
  CallJavaScriptFunction(frame, "passwords.fillPasswordForm",
                         base::Value::List()
                             .Append(std::move(form_value))
                             .Append(username)
                             .Append(password),
                         std::move(callback),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

std::optional<std::string>
PasswordManagerJavaScriptFeature::GetScriptMessageHandlerName() const {
  return FormSubmittedHandlerName;
}

void PasswordManagerJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  PasswordManagerTabHelper::GetOrCreateForWebState(web_state)
      ->ScriptMessageReceived(message);
}

void PasswordManagerJavaScriptFeature::FillPasswordForm(
    web::WebFrame* frame,
    autofill::FormRendererId form_identifier,
    autofill::FieldRendererId new_password_identifier,
    autofill::FieldRendererId confirm_password_identifier,
    NSString* generated_password,
    base::OnceCallback<void(BOOL)> callback) {
  DCHECK(!callback.is_null());
  CallJavaScriptFunction(
      frame, "passwords.fillPasswordFormWithGeneratedPassword",
      base::Value::List()
          .Append(FormRendererIdToJsParameter(form_identifier))
          .Append(FieldRendererIdToJsParameter(new_password_identifier))
          .Append(FieldRendererIdToJsParameter(confirm_password_identifier))
          .Append(base::SysNSStringToUTF8(generated_password)),
      CreateBoolCallback(std::move(callback)),
      base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

}  // namespace password_manager
