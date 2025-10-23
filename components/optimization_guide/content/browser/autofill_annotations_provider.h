// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AUTOFILL_ANNOTATIONS_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AUTOFILL_ANNOTATIONS_PROVIDER_H_

#include "base/supports_user_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace optimization_guide {
class ConvertAIPageContentToProtoSession;
}  // namespace optimization_guide

namespace optimization_guide {

// This interface enables adding Autofill information to the Annotated Page
// Contents for a given form control.
class AutofillAnnotationsProvider : public base::SupportsUserData::Data {
 public:
  AutofillAnnotationsProvider() = default;
  ~AutofillAnnotationsProvider() override = default;

  // Gets the `AutofillAnnotationsProvider` for the given `WebContents`.
  static AutofillAnnotationsProvider* GetFor(
      content::WebContents* web_contents);

  // Sets the `AutofillAnnotationsProvider` for the given `WebContents`. Takes
  // ownership of the provider.
  static void SetFor(content::WebContents* web_contents,
                     std::unique_ptr<AutofillAnnotationsProvider> provider);

  // Adds `autofill_section_id` and `coarse_autofill_field_type` to
  // `proto_attributes` for a form control. `render_frame_host` needs to be the
  // RFH that contains the form control.
  virtual void AddAutofillAnnotations(
      content::RenderFrameHost& render_frame_host,
      ConvertAIPageContentToProtoSession& session,
      optimization_guide::proto::ContentAttributes* proto_attributes) = 0;

 private:
  // The key for storing the `AutofillAnnotationsProvider` in a `WebContents`.
  static const void* UserDataKey();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AUTOFILL_ANNOTATIONS_PROVIDER_H_
