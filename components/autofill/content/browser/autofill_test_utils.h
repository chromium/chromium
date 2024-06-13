// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_TEST_UTILS_H_

#include <vector>

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill {

class FormData;
class FormFieldData;

namespace test {

// Creates a `FormData` whose `host_frame` and the host frames of its fields
// correspond to `rfh`.
FormData CreateFormDataForRenderFrameHost(content::RenderFrameHost& rfh,
                                          std::vector<FormFieldData> fields);

}  // namespace test

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_TEST_UTILS_H_
