// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_FORMS_WITH_SERVER_PREDICTIONS_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_FORMS_WITH_SERVER_PREDICTIONS_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/global_routing_id.h"

namespace autofill {

class AutofillManager;

// A helper struct that translates an Autofill browser form (i.e. potentially
// the flattening of multiple renderer forms, see
// `components/autofill/content/browser/form_forest.h` for more details) to its
// renderer form constituents while also separating out server predictions and
// the ids of the render frame hosts hosting each renderer form.
//
// Note: This code is intended only for use outside of `components/autofill`.
// Consumers of Autofill information that use their own renderer component
// instead of relying on Autofill methods may require it to map between the
// different concepts of browser (flattened) forms and renderer forms.
struct RendererFormsWithServerPredictions {
  static std::optional<RendererFormsWithServerPredictions> FromBrowserForm(
      AutofillManager& manager,
      FormGlobalId form_id);

  RendererFormsWithServerPredictions();
  RendererFormsWithServerPredictions(
      const RendererFormsWithServerPredictions&) = delete;
  RendererFormsWithServerPredictions& operator=(
      const RendererFormsWithServerPredictions&) = delete;
  RendererFormsWithServerPredictions(RendererFormsWithServerPredictions&&);
  RendererFormsWithServerPredictions& operator=(
      RendererFormsWithServerPredictions&&);
  ~RendererFormsWithServerPredictions();

  // The renderer forms together with the id of the RenderFrameHost hosting
  // them.
  std::vector<std::pair<FormData, content::GlobalRenderFrameHostId>>
      renderer_forms;
  base::flat_map<FieldGlobalId, AutofillType::ServerPrediction> predictions;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_RENDERER_FORMS_WITH_SERVER_PREDICTIONS_H_
