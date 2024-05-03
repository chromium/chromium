// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_

#import <stdint.h>

#import <vector>

#import "components/autofill/core/common/form_data_predictions.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/unique_ids.h"

namespace autofill {
class FormStructure;
}

namespace web {
class WebState;
class WebFrame;
}

// Interface used to pipe form data from AutofillDriverIOS to the embedder.
@protocol AutofillDriverIOSBridge

- (void)fillData:(const std::vector<autofill::FormFieldData::FillData>&)form
         inFrame:(web::WebFrame*)frame;

- (void)fillSpecificFormField:(const autofill::FieldRendererId&)field
                    withValue:(const std::u16string)value
                      inFrame:(web::WebFrame*)frame;

- (void)handleParsedForms:
            (const std::vector<
                raw_ptr<autofill::FormStructure, VectorExperimental>>&)forms
                  inFrame:(web::WebFrame*)frame;

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame;

// Triggers the extraction and parsing pipeline for forms in the given
// `webFrame`.
- (void)scanFormsInWebState:(web::WebState*)webState
                    inFrame:(web::WebFrame*)webFrame;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
