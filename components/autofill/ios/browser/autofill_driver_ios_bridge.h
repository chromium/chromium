// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_

#import <stdint.h>

#import <optional>
#import <string>
#import <vector>

#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_data_predictions.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/core/common/unique_ids.h"

namespace autofill {
class FormStructure;
class Section;
}

namespace web {
class WebState;
class WebFrame;
}

using FormFetchCompletion =
    base::OnceCallback<void(std::optional<std::vector<autofill::FormData>>)>;

// Interface used to pipe form data from AutofillDriverIOS to the embedder.
@protocol AutofillDriverIOSBridge

// All `fields` must come from `section` (i.e., `AutofillField::section() ==
// section`).
// The implementor may store the section to later on identify fields that were
// filled together. That is used to implement "Clear Form".
//
// TODO(crbug.com/338201947): Remove `section` when iOS replaces "Clear Form"
// with "Undo Autofill".
- (void)fillData:(const std::vector<autofill::FormFieldData::FillData>&)fields
           section:(const autofill::Section&)section
           inFrame:(web::WebFrame*)frame
    withActionType:(autofill::mojom::FormActionType)actionType;

- (void)fillSpecificFormField:(const autofill::FieldRendererId&)field
                    withValue:(const std::u16string)value
                      inFrame:(web::WebFrame*)frame;

- (void)handleParsedForms:
            (const std::vector<raw_ref<const autofill::FormStructure>>&)forms
                  inFrame:(web::WebFrame*)frame;

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame;

// Fetches autofill forms in the `frame`'s document. If `formNameFilter` is not
// `std::nullopt`, then it only provides forms whose name matches
// `*form_name_filter`.
- (void)fetchFormsFiltered:(std::optional<std::u16string>)formNameFilter
                   inFrame:(web::WebFrame*)frame
         completionHandler:(FormFetchCompletion)completionHandler;

// Notifies about the forms that were seen on the page when fetching.
- (void)notifyFormsSeen:(const std::vector<autofill::FormData>&)updatedForms
                inFrame:(web::WebFrame*)frame;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
