// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_autofill_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#import "components/autofill/ios/browser/autofill_util.h"
#include "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FieldRendererId;
using autofill::FormRendererId;

@implementation JsAutofillManager

- (void)addJSDelayInFrame:(web::WebFrame*)frame {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          autofill::switches::kAutofillIOSDelayBetweenFields)) {
    std::string delayString = command_line->GetSwitchValueASCII(
        autofill::switches::kAutofillIOSDelayBetweenFields);
    int commandLineDelay = 0;
    if (base::StringToInt(delayString, &commandLineDelay)) {
      std::vector<base::Value> parameters;
      parameters.push_back(base::Value(commandLineDelay));
      autofill::ExecuteJavaScriptFunction("autofill.setDelay", parameters,
                                          frame,
                                          autofill::JavaScriptResultCallback());
    }
  }
}

- (void)fetchFormsWithMinimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                                         inFrame:(web::WebFrame*)frame
                               completionHandler:
                                   (void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);

  bool restrictUnownedFieldsToFormlessCheckout = base::FeatureList::IsEnabled(
      autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout);
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(static_cast<int>(requiredFieldsCount)));
  parameters.push_back(base::Value(restrictUnownedFieldsToFormlessCheckout));
  autofill::ExecuteJavaScriptFunction(
      "autofill.extractForms", parameters, frame,
      autofill::CreateStringCallback(completionHandler));
}

#pragma mark -
#pragma mark ProtectedMethods

- (void)fillActiveFormField:(std::unique_ptr<base::Value>)data
                    inFrame:(web::WebFrame*)frame
          completionHandler:(void (^)(BOOL))completionHandler {
  DCHECK(data);

  bool useRendererIDs = base::FeatureList::IsEnabled(
      autofill::features::kAutofillUseUniqueRendererIDsOnIOS);
  std::string fillingFunction =
      useRendererIDs ? "autofill.fillActiveFormFieldUsingRendererIDs"
                     : "autofill.fillActiveFormField";

  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  autofill::ExecuteJavaScriptFunction(
      fillingFunction, parameters, frame,
      autofill::CreateBoolCallback(completionHandler));
}

- (void)toggleTrackingFormMutations:(BOOL)state inFrame:(web::WebFrame*)frame {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(state ? 200 : 0));
  autofill::ExecuteJavaScriptFunction("formHandlers.trackFormMutations",
                                      parameters, frame,
                                      autofill::JavaScriptResultCallback());
}

- (void)toggleTrackingUserEditedFields:(BOOL)state
                               inFrame:(web::WebFrame*)frame {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(static_cast<bool>(state)));
  autofill::ExecuteJavaScriptFunction(
      "formHandlers.toggleTrackingUserEditedFields", parameters, frame,
      autofill::JavaScriptResultCallback());
}

- (void)fillForm:(std::unique_ptr<base::Value>)data
    forceFillFieldIdentifier:(NSString*)forceFillFieldIdentifier
      forceFillFieldUniqueID:(FieldRendererId)forceFillFieldUniqueID
                     inFrame:(web::WebFrame*)frame
           completionHandler:(void (^)(NSString*))completionHandler {
  DCHECK(data);
  DCHECK(completionHandler);

  bool useRendererIDs = base::FeatureList::IsEnabled(
      autofill::features::kAutofillUseUniqueRendererIDsOnIOS);

  std::string fieldStringID =
      forceFillFieldIdentifier
          ? base::SysNSStringToUTF8(forceFillFieldIdentifier)
          : "null";
  int fieldNumericID = forceFillFieldUniqueID ? forceFillFieldUniqueID.value()
                                              : autofill::kNotSetRendererID;
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  parameters.push_back(base::Value(fieldStringID));
  parameters.push_back(base::Value(fieldNumericID));
  parameters.push_back(base::Value(useRendererIDs));
  autofill::ExecuteJavaScriptFunction(
      "autofill.fillForm", parameters, frame,
      autofill::CreateStringCallback(completionHandler));
}

- (void)clearAutofilledFieldsForFormName:(NSString*)formName
                            formUniqueID:(FormRendererId)formRendererID
                         fieldIdentifier:(NSString*)fieldIdentifier
                           fieldUniqueID:(FieldRendererId)fieldRendererID
                                 inFrame:(web::WebFrame*)frame
                       completionHandler:
                           (void (^)(NSString*))completionHandler {
  DCHECK(completionHandler);

  bool useRendererIDs = base::FeatureList::IsEnabled(
      autofill::features::kAutofillUseUniqueRendererIDsOnIOS);
  int formNumericID =
      formRendererID ? formRendererID.value() : autofill::kNotSetRendererID;
  int fieldNumericID =
      fieldRendererID ? fieldRendererID.value() : autofill::kNotSetRendererID;

  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(base::SysNSStringToUTF8(formName)));
  parameters.push_back(base::Value(formNumericID));
  parameters.push_back(base::Value(base::SysNSStringToUTF8(fieldIdentifier)));
  parameters.push_back(base::Value(fieldNumericID));
  parameters.push_back(base::Value(useRendererIDs));
  autofill::ExecuteJavaScriptFunction(
      "autofill.clearAutofilledFields", parameters, frame,
      autofill::CreateStringCallback(completionHandler));
}

- (void)fillPredictionData:(std::unique_ptr<base::Value>)data
                   inFrame:(web::WebFrame*)frame {
  DCHECK(data);
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  autofill::ExecuteJavaScriptFunction("autofill.fillPredictionData", parameters,
                                      frame,
                                      autofill::JavaScriptResultCallback());
}

@end
