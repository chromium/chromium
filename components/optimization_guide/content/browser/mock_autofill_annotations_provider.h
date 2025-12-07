// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MOCK_AUTOFILL_ANNOTATIONS_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MOCK_AUTOFILL_ANNOTATIONS_PROVIDER_H_

#include "components/optimization_guide/content/browser/autofill_annotations_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

class MockAutofillAnnotationsProvider : public AutofillAnnotationsProvider {
 public:
  MockAutofillAnnotationsProvider();
  ~MockAutofillAnnotationsProvider() override;

  MOCK_METHOD(std::optional<AutofillFieldMetadata>,
              GetAutofillFieldData,
              (content::RenderFrameHost&,
               int32_t,
               ConvertAIPageContentToProtoSession&),
              (override));
  MOCK_METHOD(AutofillAvailability,
              GetAutofillAvailability,
              (content::RenderFrameHost&),
              (override));
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_MOCK_AUTOFILL_ANNOTATIONS_PROVIDER_H_
