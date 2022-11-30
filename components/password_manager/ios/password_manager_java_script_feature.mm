// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_manager_java_script_feature.h"

#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::CreateBoolCallback;
using autofill::CreateStringCallback;

namespace password_manager {

namespace {
constexpr char kScriptName[] = "password_controller";

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

std::unique_ptr<base::Value> SerializeFillData(
    const GURL& origin,
    autofill::FormRendererId form_renderer_id,
    autofill::FieldRendererId username_element,
    const std::u16string& username_value,
    autofill::FieldRendererId password_element,
    const std::u16string& password_value) {
  auto root_dict = std::make_unique<base::DictionaryValue>();
  root_dict->SetString("origin", origin.spec());
  root_dict->SetInteger("unique_renderer_id",
                        FormRendererIdToJsParameter(form_renderer_id));

  base::Value::List fieldList;

  base::Value::Dict usernameField;
  usernameField.Set("unique_renderer_id",
                    FieldRendererIdToJsParameter(username_element));
  usernameField.Set("value", username_value);
  fieldList.Append(std::move(usernameField));

  base::Value::Dict passwordField;
  passwordField.Set("unique_renderer_id",
                    static_cast<int>(password_element.value()));
  passwordField.Set("value", password_value);
  fieldList.Append(std::move(passwordField));

  root_dict->GetDict().Set("fields", std::move(fieldList));

  return root_dict;
}

// Serializes |fill_data| so it can be used by the JS side of
// PasswordController. Includes both username and password data if
// |fill_username|, and only password data otherwise.
std::unique_ptr<base::Value> SerializeFillData(
    const password_manager::FillData& fill_data,
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
          // TODO(crbug.com/1175793): Move autofill code to kAnyContentWorld
          // once all scripts are converted to JavaScriptFeatures.
          ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature(),
           web::java_script_features::GetMessageJavaScriptFeature(),
           autofill::FormUtilJavaScriptFeature::GetInstance()}) {}

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
  std::vector<base::Value> parameters;
  parameters.emplace_back(FormRendererIdToJsParameter(form_identifier));
  CallJavaScriptFunction(frame, "passwords.getPasswordFormDataAsString",
                         parameters, CreateStringCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void PasswordManagerJavaScriptFeature::FillPasswordForm(
    web::WebFrame* frame,
    const password_manager::FillData& fill_data,
    BOOL fill_username,
    const std::string& username,
    const std::string& password,
    base::OnceCallback<void(BOOL)> callback) {
  DCHECK(!callback.is_null());

  std::unique_ptr<base::Value> form_value =
      SerializeFillData(fill_data, fill_username);

  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*form_value));
  parameters.emplace_back(std::move(username));
  parameters.emplace_back(std::move(password));
  CallJavaScriptFunction(frame, "passwords.fillPasswordForm", parameters,
                         CreateBoolCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

void PasswordManagerJavaScriptFeature::FillPasswordForm(
    web::WebFrame* frame,
    autofill::FormRendererId form_identifier,
    autofill::FieldRendererId new_password_identifier,
    autofill::FieldRendererId confirm_password_identifier,
    NSString* generated_password,
    base::OnceCallback<void(BOOL)> callback) {
  DCHECK(!callback.is_null());
  std::vector<base::Value> parameters;
  parameters.emplace_back(FormRendererIdToJsParameter(form_identifier));
  parameters.emplace_back(
      FieldRendererIdToJsParameter(new_password_identifier));
  parameters.emplace_back(
      FieldRendererIdToJsParameter(confirm_password_identifier));
  parameters.push_back(
      base::Value(base::SysNSStringToUTF8(generated_password)));
  CallJavaScriptFunction(frame,
                         "passwords.fillPasswordFormWithGeneratedPassword",
                         parameters, CreateBoolCallback(std::move(callback)),
                         base::Seconds(kJavaScriptExecutionTimeoutInSeconds));
}

}  // namespace password_manager
