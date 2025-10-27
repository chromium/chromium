// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_GLIC_AUTOFILL_ANNOTATIONS_PROVIDER_IMPL_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_GLIC_AUTOFILL_ANNOTATIONS_PROVIDER_IMPL_H_

#include "components/optimization_guide/content/browser/autofill_annotations_provider.h"

namespace optimization_guide {

// This class is to be initiated and registered by the AutofillDriverFactory.
class AutofillAnnotationsProviderImpl : public AutofillAnnotationsProvider {
 public:
  ~AutofillAnnotationsProviderImpl() override;

  // AutofillAnnotationsProvider:
  std::optional<AutofillFieldMetadata> GetAutofillFieldData(
      content::RenderFrameHost& render_frame_host,
      int32_t dom_node_id,
      ConvertAIPageContentToProtoSession& session) override;
  AutofillAvailability GetAutofillAvailability(
      content::RenderFrameHost& render_frame_host) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_GLIC_AUTOFILL_ANNOTATIONS_PROVIDER_IMPL_H_
