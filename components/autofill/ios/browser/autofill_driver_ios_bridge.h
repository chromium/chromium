// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_

#import <stdint.h>

#import <vector>

#import "components/autofill/core/common/form_data.h"
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

using FormFetchCompletion =
    base::OnceCallback<void(std::optional<std::vector<autofill::FormData>>)>;

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

// Fetches autofill forms in the `frame`'s document. Only provides the first
// form matching `formName` if `filtered` is true.
- (void)fetchFormsFiltered:(BOOL)filtered
                  withName:(const std::u16string&)formName
                   inFrame:(web::WebFrame*)frame
         completionHandler:(FormFetchCompletion)completionHandler;

// Notifies about the forms that were seen on the page when fetching.
- (void)notifyFormsSeen:(const std::vector<autofill::FormData>&)updatedForms
                inFrame:(web::WebFrame*)frame;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
