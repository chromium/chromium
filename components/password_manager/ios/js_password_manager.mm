// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/js_password_manager.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/ios/account_select_fill_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace password_manager {

NSString* SerializeFillData(const GURL& origin,
                            const GURL& action,
                            const base::string16& username_element,
                            const base::string16& username_value,
                            const base::string16& password_element,
                            const base::string16& password_value) {
  base::DictionaryValue rootDict;
  rootDict.SetString("origin", origin.spec());
  rootDict.SetString("action", action.spec());

  auto fieldList = std::make_unique<base::ListValue>();

  auto usernameField = std::make_unique<base::DictionaryValue>();
  usernameField->SetString("name", username_element);
  usernameField->SetString("value", username_value);
  fieldList->Append(std::move(usernameField));

  auto passwordField = std::make_unique<base::DictionaryValue>();
  passwordField->SetString("name", password_element);
  passwordField->SetString("value", password_value);
  fieldList->Append(std::move(passwordField));

  rootDict.Set("fields", std::move(fieldList));

  std::string jsonString;
  base::JSONWriter::Write(rootDict, &jsonString);
  return base::SysUTF8ToNSString(jsonString);
}

NSString* SerializePasswordFormFillData(
    const autofill::PasswordFormFillData& formData) {
  return SerializeFillData(
      formData.origin, formData.action, formData.username_field.name,
      formData.username_field.value, formData.password_field.name,
      formData.password_field.value);
}

NSString* SerializeFillData(const password_manager::FillData& fillData) {
  return SerializeFillData(fillData.origin, fillData.action,
                           fillData.username_element, fillData.username_value,
                           fillData.password_element, fillData.password_value);
}

}  // namespace password_manager

namespace {
// Sanitizes |JSONString| and wraps it in quotes so it can be injected safely in
// JavaScript.
NSString* JSONEscape(NSString* JSONString) {
  return base::SysUTF8ToNSString(
      base::GetQuotedJSONString(base::SysNSStringToUTF8(JSONString)));
}
}  // namespace

@implementation JsPasswordManager {
  // The injection receiver used to evaluate JavaScript.
  __weak CRWJSInjectionReceiver* _receiver;
}

- (instancetype)initWithReceiver:(CRWJSInjectionReceiver*)receiver {
  DCHECK(receiver);
  self = [super init];
  if (self) {
    _receiver = receiver;
  }
  return self;
}

- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  [_receiver executeJavaScript:@"__gCrWeb.passwords.findPasswordForms()"
             completionHandler:^(id result, NSError*) {
               completionHandler(base::mac::ObjCCastStrict<NSString>(result));
             }];
}

- (void)extractForm:(NSString*)formName
      completionHandler:(void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);
  NSString* extra = [NSString
      stringWithFormat:@"__gCrWeb.passwords.getPasswordFormDataAsString(%@)",
                       JSONEscape(formName)];
  [_receiver executeJavaScript:extra
             completionHandler:^(id result, NSError*) {
               completionHandler(base::mac::ObjCCastStrict<NSString>(result));
             }];
}

- (void)fillPasswordForm:(NSString*)JSONString
            withUsername:(NSString*)username
                password:(NSString*)password
       completionHandler:(void (^)(BOOL))completionHandler {
  DCHECK(completionHandler);
  NSString* script = [NSString
      stringWithFormat:@"__gCrWeb.passwords.fillPasswordForm(%@, %@, %@)",
                       JSONString, JSONEscape(username), JSONEscape(password)];
  [_receiver executeJavaScript:script
             completionHandler:^(id result, NSError*) {
               completionHandler([result isEqual:@YES]);
             }];
}

- (void)fillPasswordForm:(NSString*)formName
        newPasswordIdentifier:(NSString*)newPasswordIdentifier
    confirmPasswordIdentifier:(NSString*)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(void (^)(BOOL))completionHandler {
  NSString* script = [NSString
      stringWithFormat:@"__gCrWeb.passwords."
                       @"fillPasswordFormWithGeneratedPassword(%@, %@, %@, %@)",
                       JSONEscape(formName), JSONEscape(newPasswordIdentifier),
                       JSONEscape(confirmPasswordIdentifier),
                       JSONEscape(generatedPassword)];
  [_receiver executeJavaScript:script
             completionHandler:^(id result, NSError*) {
               completionHandler([result isEqual:@YES]);
             }];
}

@end
