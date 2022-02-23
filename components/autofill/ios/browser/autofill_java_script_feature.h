// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_JAVA_SCRIPT_FEATURE_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

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

  // Adds a delay between filling the form fields in frame.
  void AddJSDelayInFrame(web::WebFrame* frame);

  // Extracts forms from a web |frame|. Only forms with at least
  // |requiredFieldsCount| fields are extracted. |callback| is called with the
  // JSON string of forms of a web page. |callback| cannot be null.
  void FetchForms(web::WebFrame* frame,
                  NSUInteger requiredFieldsCount,
                  base::OnceCallback<void(NSString*)> callback);

  // Fills the data in JSON string |dataString| into the active form field in
  // |frame|, then executes the |completionHandler|.
  void FillActiveFormField(web::WebFrame* frame,
                           std::unique_ptr<base::DictionaryValue> data,
                           base::OnceCallback<void(BOOL)> callback);

  // Fills a number of fields in the same named form for full-form Autofill.
  // Applies Autofill CSS (i.e. yellow background) to filled elements.
  // Only empty fields will be filled, except that field named
  // Field identified by |forceFillFieldID| will always be filled even if
  // non-empty. |forceFillFieldID| may be null. Fields must be contained in
  // |frame|. |completionHandler| is called after the forms are filled with the
  // JSON string containing pairs of unique renderer ids of filled fields and
  // corresponding filled values. |completionHandler| cannot be nil.
  void FillForm(web::WebFrame* frame,
                std::unique_ptr<base::Value> data,
                autofill::FieldRendererId forceFillFieldID,
                base::OnceCallback<void(NSString*)> callback);

  // Clear autofilled fields of the specified form and frame. Fields that are
  // not currently autofilled are not modified. Field contents are cleared, and
  // Autofill flag and styling are removed. 'change' events are sent for fields
  // whose contents changed.
  // |fieldUniqueID| identifies the field that initiated the
  // clear action. |completionHandler| is called after the forms are filled with
  // the JSON string containing a list of unique renderer ids of cleared fields.
  // |completionHandler| cannot be nil.
  void ClearAutofilledFieldsForForm(
      web::WebFrame* frame,
      autofill::FormRendererId formRendererID,
      autofill::FieldRendererId fieldRendererID,
      base::OnceCallback<void(NSString*)> callback);

  // Marks up the form with autofill field prediction data (diagnostic tool).
  void FillPredictionData(web::WebFrame* frame,
                          std::unique_ptr<base::Value> data);

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
