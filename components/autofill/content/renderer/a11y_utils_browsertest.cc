// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/a11y_utils.h"

#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"

namespace autofill {

using A11yUtilsTest = content::RenderViewTest;

TEST_F(A11yUtilsTest, SetAutofillSuggestionAvailability) {
  LoadHTML("<input id='input_id'>");
  blink::WebDocument document = GetMainFrame()->GetDocument();

  // Creating context imitates a screen reader enabled, so that all ax nodes
  // are created and attributes/state are updatable.
  auto ax_context = std::make_unique<blink::WebAXContext>(
      document, ui::AXMode::kScreenReader);
  ax_context->UpdateAXForAllDocuments();

  blink::WebInputElement element =
      document.GetElementById("input_id").To<blink::WebInputElement>();
  blink::WebAXObject element_ax_object =
      blink::WebAXObject::FromWebNode(element);

  // kNoSuggestions by default.
  ui::AXNodeData node_data;
  element_ax_object.Serialize(&node_data, ui::AXMode::kScreenReader);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kAutofillAvailable));
  EXPECT_FALSE(
      node_data.HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete));

  // kAutofillAvailable.
  SetAutofillSuggestionAvailability(
      element, mojom::AutofillSuggestionAvailability::kAutofillAvailable);
  ax_context->UpdateAXForAllDocuments();

  node_data = ui::AXNodeData();
  element_ax_object.Serialize(&node_data, ui::AXMode::kScreenReader);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kAutofillAvailable));
  EXPECT_FALSE(
      node_data.HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete));

  // kAutocompleteAvailable.
  SetAutofillSuggestionAvailability(
      element, mojom::AutofillSuggestionAvailability::kAutocompleteAvailable);
  ax_context->UpdateAXForAllDocuments();

  node_data = ui::AXNodeData();
  element_ax_object.Serialize(&node_data, ui::AXMode::kScreenReader);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kAutofillAvailable));
  EXPECT_TRUE(
      node_data.HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete));

  // kNoSuggestions.
  SetAutofillSuggestionAvailability(
      element, mojom::AutofillSuggestionAvailability::kNoSuggestions);
  ax_context->UpdateAXForAllDocuments();

  node_data = ui::AXNodeData();
  element_ax_object.Serialize(&node_data, ui::AXMode::kScreenReader);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kAutofillAvailable));
  EXPECT_FALSE(
      node_data.HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete));
}

}  // namespace autofill
