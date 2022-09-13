// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_

#include <stdint.h>

#include <vector>

#include "components/autofill/core/common/form_data_predictions.h"

namespace autofill {
struct FormData;
class FormStructure;
}

namespace web {
class WebFrame;
}

// Interface used to pipe form data from AutofillDriverIOS to the embedder.
@protocol AutofillDriverIOSBridge

- (void)fillFormData:(const autofill::FormData&)form
             inFrame:(web::WebFrame*)frame;

- (void)handleParsedForms:(const std::vector<autofill::FormStructure*>&)forms
                  inFrame:(web::WebFrame*)frame;

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_BRIDGE_H_
