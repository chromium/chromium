// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_JS_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_JS_PASSWORD_MANAGER_H_

#include "base/ios/block_types.h"
#include "components/autofill/core/common/unique_ids.h"
#include "ios/web/public/js_messaging/web_frame.h"

namespace autofill {
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {

struct FillData;

// Serializes |fillData| so it can be used by the JS side of PasswordController.
// Includes both username and password data if |fillUsername|, and only password
// data otherwise.
std::unique_ptr<base::Value> SerializeFillData(
    const password_manager::FillData& fillData,
    BOOL fillUsername);

// Serializes |formData| so it can be used by the JS side of PasswordController.
std::unique_ptr<base::Value> SerializePasswordFormFillData(
    const autofill::PasswordFormFillData& formData);

}  // namespace password_manager

// Loads the JavaScript file, password_controller.js, which contains password
// form parsing and autofill functions. It will be evaluated on a page that
// is known to have at least one password form (see hasPasswordField_ in
// password_controller.js) It returns contents of those password forms and also
// registers functions that are later used to autofill them.
@interface JsPasswordManager : NSObject

// Finds any password forms on the web page.
// |completionHandler| is then called with the JSON string result (which can
// be a zero-length string if there was an error). |completionHandler| cannot be
// nil.
// For example the JSON string for a form with a single password field is:
// [{"action":null,"method":null,"usernameElement":"","usernameValue":"","
// passwords":[{"element":"","value":"asd"}]}]
- (void)findPasswordFormsInFrame:(web::WebFrame*)frame
               completionHandler:(void (^)(NSString*))completionHandler;

// Extracts the password form with the given name from a web page.
// |completionHandler| is called with the JSON string containing the info about
// submitted password forms from a web page (it can be zero-length if there was
// an error). |completionHandler| cannot be nil.
// For example. the JSON string for a form with a single password field is:
// {"action":null,"method":null,"usernameElement":"","usernameValue":"",
// "passwords":[{"element":"","value":"asd"}]}
- (void)extractForm:(autofill::FormRendererId)formIdentifier
              inFrame:(web::WebFrame*)frame
    completionHandler:(void (^)(NSString*))completionHandler;

// Fills in the password form specified by |JSONString| with the given
// |username| and |password|. Assumes JavaScript has been injected previously
// by calling |findPasswordFormsWithCompletionHandle| or
// |extractSubmittedFormWithCompletionHandler|. Calls |completionHandler| with
// YES if the filling of the password was successful, NO otherwise.
// |completionHandler| cannot be nil.
- (void)fillPasswordForm:(std::unique_ptr<base::Value>)form
                 inFrame:(web::WebFrame*)frame
            withUsername:(std::string)username
                password:(std::string)password
       completionHandler:(void (^)(BOOL))completionHandler;

// Fills new password field for (optional) |newPasswordIdentifier| and for
// (optional) confirm password field |confirmPasswordIdentifier| in the form
// identified by |formData|. Invokes |completionHandler| with true if any fields
// were filled, false otherwise.
- (void)fillPasswordForm:(autofill::FormRendererId)formIdentifier
                      inFrame:(web::WebFrame*)frame
        newPasswordIdentifier:(autofill::FieldRendererId)newPasswordIdentifier
    confirmPasswordIdentifier:
        (autofill::FieldRendererId)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(void (^)(BOOL))completionHandler;

// Sets up the next available unique ID value in a document.
- (void)setUpForUniqueIDsWithInitialState:(uint32_t)nextAvailableID
                                  inFrame:(web::WebFrame*)frame;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_JS_PASSWORD_MANAGER_H_
