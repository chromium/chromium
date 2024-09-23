// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_JAVA_SCRIPT_FEATURE_H_

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

namespace password_manager {

struct FillData;

// Communicates with the JavaScript file password_controller.js, which
// contains password form parsing and autofill functions.
class PasswordManagerJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static PasswordManagerJavaScriptFeature* GetInstance();

  // Finds any password forms on the web page.
  // |callback| is then called with the JSON string result (which can
  // be a zero-length string if there was an error). |callback| cannot be null.
  // For example the JSON string for a form with a single password field is:
  // [{"action":null,"method":null,"usernameElement":"","usernameValue":"","
  // passwords":[{"element":"","value":"asd"}]}]
  void FindPasswordFormsInFrame(web::WebFrame* frame,
                                base::OnceCallback<void(NSString*)> callback);

  // Extracts the password form with the given |form_identifier| from a web
  // page. |callback| is called with the JSON string containing the info about
  // submitted password forms from a web page (it can be zero-length if there
  // was an error). |callback| cannot be null. For example. the JSON string for
  // a form with a single password field is:
  // {"action":null,"method":null,"usernameElement":"","usernameValue":"",
  // "passwords":[{"element":"","value":"asd"}]}
  void ExtractForm(web::WebFrame* frame,
                   autofill::FormRendererId form_identifier,
                   base::OnceCallback<void(NSString*)> callback);

  // Fills in the form specified by |form| with the given |password|.
  // |username| will be filled in if and only if |fill_username| is true.
  // Assumes JavaScript has been injected previously by calling
  // |FindPasswordFormsInFrame| or |ExtractForm|. Calls |callback|
  // with YES if the filling of the password was successful, NO otherwise.
  // |callback| cannot be null.
  void FillPasswordForm(web::WebFrame* frame,
                        const password_manager::FillData& form,
                        BOOL fill_username,
                        const std::string& username,
                        const std::string& password,
                        base::OnceCallback<void(const base::Value*)> callback);

  // Fills new password field for (optional) |new_password_identifier| and for
  // (optional) confirm password field |confirm_password_identifier| in the form
  // identified by |form_identifier|. Invokes |callback| with YES if any fields
  // were filled, false otherwise.
  void FillPasswordForm(web::WebFrame* frame,
                        autofill::FormRendererId form_identifier,
                        autofill::FieldRendererId new_password_identifier,
                        autofill::FieldRendererId confirm_password_identifier,
                        NSString* generated_password,
                        base::OnceCallback<void(BOOL)> callback);

 private:
  friend class base::NoDestructor<PasswordManagerJavaScriptFeature>;

  PasswordManagerJavaScriptFeature();
  ~PasswordManagerJavaScriptFeature() override;

  PasswordManagerJavaScriptFeature(const PasswordManagerJavaScriptFeature&) =
      delete;
  PasswordManagerJavaScriptFeature& operator=(
      const PasswordManagerJavaScriptFeature&) = delete;

  // web::JavaScriptFeature
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // Calls the "passwords.fillPasswordForm" JavaScript function to fill the form
  // described by |form_value| with |username| and |password|.
  void FillPasswordForm(web::WebFrame* frame,
                        std::unique_ptr<base::Value> form_value,
                        const std::string& username,
                        const std::string& password,
                        base::OnceCallback<void(base::Value*)> callback);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_JAVA_SCRIPT_FEATURE_H_
