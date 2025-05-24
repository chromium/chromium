// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_FORMS_FROM_BROWSER_FORM_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_FORMS_FROM_BROWSER_FORM_H_

#include <optional>
#include <vector>

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace autofill {

class AutofillManager;

// Finds the RenderFrameHost in web_contents with the given `frame_token` or
// returns nullptr if it does not exist.
content::RenderFrameHost* FindRenderFrameHostByToken(
    content::WebContents& web_contents,
    const LocalFrameToken& frame_token);

// The renderer forms together with the id of the RenderFrameHost hosting
// them.
// This is a helper method that translates an Autofill browser form (i.e.
// potentially the flattening of multiple renderer forms, see
// `components/autofill/content/browser/form_forest.h` for more details) to its
// renderer form constituents while also separating out
// the ids of the render frame hosts hosting each renderer form.
//
// Note: This code is intended only for use outside of `components/autofill`.
// Consumers of Autofill information that use their own renderer component
// instead of relying on Autofill methods may require it to map between the
// different concepts of browser (flattened) forms and renderer forms.
using RendererForms =
    std::vector<std::pair<FormData, content::GlobalRenderFrameHostId>>;

std::optional<RendererForms> RendererFormsFromBrowserForm(
    AutofillManager& manager,
    FormGlobalId form_id);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_FORMS_FROM_BROWSER_FORM_H_
