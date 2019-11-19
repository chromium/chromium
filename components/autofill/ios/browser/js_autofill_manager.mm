// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_autofill_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
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

@implementation JsAutofillManager {
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
      autofill::ExecuteJavaScriptFunction(
          "autofill.setDelay", parameters, frame, _receiver,
          base::OnceCallback<void(NSString*)>());
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
  autofill::ExecuteJavaScriptFunction("autofill.extractForms", parameters,
                                      frame, _receiver,
                                      base::BindOnce(completionHandler));
}

#pragma mark -
#pragma mark ProtectedMethods

- (void)fillActiveFormField:(std::unique_ptr<base::Value>)data
                    inFrame:(web::WebFrame*)frame
          completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(data);
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  autofill::ExecuteJavaScriptFunction("autofill.fillActiveFormField",
                                      parameters, frame, _receiver,
                                      base::BindOnce(^(NSString*) {
                                        completionHandler();
                                      }));
}

- (void)toggleTrackingFormMutations:(BOOL)state inFrame:(web::WebFrame*)frame {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(state ? 200 : 0));
  autofill::ExecuteJavaScriptFunction("formHandlers.trackFormMutations",
                                      parameters, frame, _receiver,
                                      base::OnceCallback<void(NSString*)>());
}

- (void)toggleTrackingUserEditedFields:(BOOL)state
                               inFrame:(web::WebFrame*)frame {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(static_cast<bool>(state)));
  autofill::ExecuteJavaScriptFunction(
      "formHandlers.toggleTrackingUserEditedFields", parameters, frame,
      _receiver, base::OnceCallback<void(NSString*)>());
}

- (void)fillForm:(std::unique_ptr<base::Value>)data
    forceFillFieldIdentifier:(NSString*)forceFillFieldIdentifier
                     inFrame:(web::WebFrame*)frame
           completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(data);
  DCHECK(completionHandler);
  std::string fieldIdentifier =
      forceFillFieldIdentifier
          ? base::SysNSStringToUTF8(forceFillFieldIdentifier)
          : "null";
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  parameters.push_back(base::Value(fieldIdentifier));
  autofill::ExecuteJavaScriptFunction("autofill.fillForm", parameters, frame,
                                      _receiver, base::BindOnce(^(NSString*) {
                                        completionHandler();
                                      }));
}

- (void)clearAutofilledFieldsForFormName:(NSString*)formName
                         fieldIdentifier:(NSString*)fieldIdentifier
                                 inFrame:(web::WebFrame*)frame
                       completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(completionHandler);
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(base::SysNSStringToUTF8(formName)));
  parameters.push_back(base::Value(base::SysNSStringToUTF8(fieldIdentifier)));
  autofill::ExecuteJavaScriptFunction("autofill.clearAutofilledFields",
                                      parameters, frame, _receiver,
                                      base::BindOnce(^(NSString*) {
                                        completionHandler();
                                      }));
}

- (void)fillPredictionData:(std::unique_ptr<base::Value>)data
                   inFrame:(web::WebFrame*)frame {
  DCHECK(data);
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(*data));
  autofill::ExecuteJavaScriptFunction("autofill.fillPredictionData", parameters,
                                      frame, _receiver,
                                      base::OnceCallback<void(NSString*)>());
}

@end
