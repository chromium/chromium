// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/renderer_forms_with_server_predictions.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_driver_router.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// static
absl::optional<RendererFormsWithServerPredictions>
RendererFormsWithServerPredictions::FromBrowserForm(AutofillManager& manager,
                                                    FormGlobalId form_id) {
  FormStructure* browser_form = manager.FindCachedFormById(form_id);
  if (!browser_form) {
    return absl::nullopt;
  }

  FormDataAndServerPredictions form_and_predictions =
      GetFormDataAndServerPredictions(*browser_form);
  RendererFormsWithServerPredictions result;
  result.predictions = std::move(form_and_predictions.predictions);
  if (base::FeatureList::IsEnabled(
          features::kAutofillPassRendererFormsToPasswordManager)) {
    // The cast is safe, since this method can only get called by content
    // embedders.
    ContentAutofillClient& client =
        static_cast<ContentAutofillClient&>(manager.client());
    const AutofillDriverRouter& router =
        client.GetAutofillDriverFactory()->router();
    std::vector<FormData> renderer_forms =
        router.GetRendererForms(form_and_predictions.form_data);

    result.renderer_forms.reserve(renderer_forms.size());
    base::flat_map<LocalFrameToken, content::GlobalRenderFrameHostId>
        token_rfh_map;
    for (FormData& form : renderer_forms) {
      content::GlobalRenderFrameHostId rfh_id;
      if (auto it = token_rfh_map.find(form.host_frame);
          it != token_rfh_map.end()) {
        rfh_id = it->second;
      } else {
        // Attempt to find the RFH with `form.host_frame` as a local frame
        // token.
        client.GetWebContents().ForEachRenderFrameHost(
            [&rfh_id, &form](content::RenderFrameHost* host) {
              if (LocalFrameToken(host->GetFrameToken().value()) ==
                  form.host_frame) {
                rfh_id = host->GetGlobalId();
              }
            });
        token_rfh_map.insert({form.host_frame, rfh_id});
      }
      if (!rfh_id) {
        continue;
      }
      result.renderer_forms.emplace_back(std::move(form), rfh_id);
    }

    return result;
  }

  ContentAutofillDriver& driver =
      static_cast<ContentAutofillDriver&>(manager.driver());
  result.renderer_forms = {{std::move(form_and_predictions.form_data),
                            driver.render_frame_host()->GetGlobalId()}};
  return result;
}

RendererFormsWithServerPredictions::RendererFormsWithServerPredictions() =
    default;

RendererFormsWithServerPredictions::RendererFormsWithServerPredictions(
    RendererFormsWithServerPredictions&&) = default;

RendererFormsWithServerPredictions&
RendererFormsWithServerPredictions::operator=(
    RendererFormsWithServerPredictions&&) = default;

RendererFormsWithServerPredictions::~RendererFormsWithServerPredictions() =
    default;

}  // namespace autofill
