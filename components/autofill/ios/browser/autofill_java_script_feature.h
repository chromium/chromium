// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_JAVA_SCRIPT_FEATURE_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "base/values.h"
#import "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace web {
class WebFrame;
}  // namespace web

namespace autofill {

// Communicates with the JavaScript file, autofill_controller.js, which contains
// form parsing and autofill functions.
class AutofillJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static AutofillJavaScriptFeature* GetInstance();

  // Extracts forms from a web `frame`. Only forms with at least
  // `required_fields_count` fields are extracted. `callback` is called
  // with the JSON string of forms of a web page.  `callback` cannot be nil.
  void FetchForms(web::WebFrame* frame,
                  base::OnceCallback<void(NSString*)> callback);

  // Fills `data` into the active form field in `frame`, then executes the
  // `callback`. `callback` cannot be nil.
  void FillActiveFormField(web::WebFrame* frame,
                           base::Value::Dict data,
                           base::OnceCallback<void(BOOL)> callback);

  // Fills `data` into the field identified by `data['renderer_id']`,
  // then executes callback. This is similar to `FillActiveFormField`, but does
  // not require that the target element be the active element.
  void FillSpecificFormField(web::WebFrame* frame,
                             base::Value::Dict data,
                             base::OnceCallback<void(BOOL)> callback);

  // Fills a number of fields in the same named form for full-form Autofill.
  // Applies Autofill CSS (i.e. yellow background) to filled elements.
  // Only empty fields will be filled, except that field named
  // Field identified by `force_fill_field_id` will always be filled even if
  // non-empty. `force_fill_field_id` may be null. Fields must be contained in
  // `frame`. `callback` is called after the forms are filled with `data`
  // which must contain pairs of unique renderer ids of filled fields and
  // corresponding filled values. `callback` cannot be nil.
  void FillForm(web::WebFrame* frame,
                base::Value::Dict data,
                autofill::FieldRendererId force_fill_field_id,
                base::OnceCallback<void(NSString*)> callback);

  // Clear autofilled fields of the specified form and frame. Fields that are
  // not currently autofilled are not modified. Field contents are cleared, and
  // Autofill flag and styling are removed. 'change' events are sent for fields
  // whose contents changed.
  // `form_renderer_id` and `field_renderer_id` identify the field that
  // initiated the clear action. `callback is called after the forms are filled
  // with the JSON string containing a list of unique renderer ids of cleared
  // fields. `callback` cannot be nil.
  void ClearAutofilledFieldsForForm(
      web::WebFrame* frame,
      autofill::FormRendererId form_renderer_id,
      autofill::FieldRendererId field_renderer_id,
      base::OnceCallback<void(NSString*)> callback);

  // Marks up the form with autofill field prediction data (diagnostic tool).
  void FillPredictionData(web::WebFrame* frame, base::Value::Dict data);

  // web::JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;

 protected:
  // web::JavaScriptFeature:
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

 private:
  friend class base::NoDestructor<AutofillJavaScriptFeature>;

  AutofillJavaScriptFeature();
  ~AutofillJavaScriptFeature() override;

  AutofillJavaScriptFeature(const AutofillJavaScriptFeature&) = delete;
  AutofillJavaScriptFeature& operator=(const AutofillJavaScriptFeature&) =
      delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_JAVA_SCRIPT_FEATURE_H_
