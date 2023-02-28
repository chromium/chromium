// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FEEDBACK_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FEEDBACK_DATA_H_

#include "base/values.h"

namespace autofill {

class AutofillManager;

namespace data_logs {

// The method extracts autofill metadata contained in the browser autofill
// manager related to form_structures, etc. It bundles these data points into a
// JSON like structure. Consider the example below a complete representation of
// the JSON structure returned by the function.
// {
//   "form_structures": [
//     {
//       "form_signature": 17674194124817378836,
//       "renderer_id": 2,
//       "host_frame": "DB42F98A7DF7AC8AFEE6B79AEFC7080B",
//       "source_url": "https://www.example.com",
//       "main_frame_url": "https://www.example.com",
//       "id_attribute": "myId",
//       "name_attribute": "myName",
//       "fields": [
//         {
//           "field_signature": 2666629521,
//           "host_form_signature": 17674194124817378836,
//           "autocomplete_attribute": "shipping name",
//           "id_attribute": "shipping_name",
//           "parseable_name_attribute": "shipping_name",
//           "label_attribute": "Name",
//           "placeholder_attribute": "Name",
//           "field_type": "HTML_TYPE_NAME",
//           "heuristic_type": "NAME_FULL",
//           "server_type": "NAME_FULL",
//           "server_type_is_override": false,
//           "html_type": "HTML_TYPE_NAME",
//           "section": "billing_fname_0_48",
//           "is_empty": true,
//           "is_focusable": true,
//           "is_visible": true
//         },
//         ...
//       ]
//     },
//     ...
//   ]
// }
base::Value::Dict FetchAutofillFeedbackData(
    AutofillManager* manager,
    base::Value::Dict extra_logs = base::Value::Dict());

}  // namespace data_logs
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FEEDBACK_DATA_H_
