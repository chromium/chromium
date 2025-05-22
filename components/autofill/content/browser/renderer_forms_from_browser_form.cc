// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/renderer_forms_from_browser_form.h"

#include <vector>

#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_driver_router.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

content::RenderFrameHost* FindRenderFrameHostByToken(
    content::WebContents& web_contents,
    const LocalFrameToken& frame_token) {
  content::RenderFrameHost* render_frame_host = nullptr;
  web_contents.ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    if (rfh->GetFrameToken().value() == frame_token.value()) {
      render_frame_host = rfh;
    }
  });
  return render_frame_host;
}

std::optional<RendererForms> RendererFormsFromBrowserForm(
    AutofillManager& manager,
    FormGlobalId form_id) {
  FormStructure* browser_form = manager.FindCachedFormById(form_id);
  if (!browser_form) {
    return std::nullopt;
  }

  // The cast is safe, since this method can only get called by content
  // embedders.
  ContentAutofillClient& client =
      static_cast<ContentAutofillClient&>(manager.client());
  const AutofillDriverRouter& router =
      client.GetAutofillDriverFactory().router();
  std::vector<FormData> renderer_forms =
      router.GetRendererForms(browser_form->ToFormData());

  RendererForms result;
  result.reserve(renderer_forms.size());
  base::flat_map<LocalFrameToken, content::GlobalRenderFrameHostId>
      token_rfh_map;
  for (FormData& form : renderer_forms) {
    content::GlobalRenderFrameHostId rfh_id;
    if (auto it = token_rfh_map.find(form.host_frame());
        it != token_rfh_map.end()) {
      rfh_id = it->second;
    } else {
      if (content::RenderFrameHost* host = FindRenderFrameHostByToken(
              client.GetWebContents(), form.host_frame())) {
        rfh_id = host->GetGlobalId();
      }
      token_rfh_map.insert({form.host_frame(), rfh_id});
    }
    if (!rfh_id) {
      continue;
    }
    result.emplace_back(std::move(form), rfh_id);
  }

  return std::move(result);
}

}  // namespace autofill
