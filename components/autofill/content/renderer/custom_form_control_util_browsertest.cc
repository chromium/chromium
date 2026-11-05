// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_util.h"

#include <vector>

#include "content/public/test/render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using CustomFormControlUtilTest = content::RenderViewTest;

// Tests that `GroupCustomFormControlsByForm()` groups custom form controls by
// their outermost ancestor `<form>` element and groups unowned controls under a
// null `blink::WebFormElement`, preserving relative order within each form.
TEST_F(CustomFormControlUtilTest, GroupCustomFormControlsByForm) {
  LoadHTML(R"(
    <div id="unowned1"></div>
    <form id="form1">
      <div id="f1_c1"></div>
      <div id="f1_c2"></div>
    </form>
    <div id="unowned2"></div>
    <form id="form2">
      <div id="f2_c1"></div>
    </form>
  )");

  blink::WebDocument doc = GetMainFrame()->GetDocument();
  CustomFormControl unowned1{.canonical_dom_node =
                                 doc.GetElementById("unowned1")};
  CustomFormControl f1_c1{.canonical_dom_node = doc.GetElementById("f1_c1")};
  CustomFormControl f1_c2{.canonical_dom_node = doc.GetElementById("f1_c2")};
  CustomFormControl unowned2{.canonical_dom_node =
                                 doc.GetElementById("unowned2")};
  CustomFormControl f2_c1{.canonical_dom_node = doc.GetElementById("f2_c1")};

  blink::WebFormElement form1 =
      doc.GetElementById("form1").To<blink::WebFormElement>();
  blink::WebFormElement form2 =
      doc.GetElementById("form2").To<blink::WebFormElement>();

  EXPECT_THAT(
      GroupCustomFormControlsByForm({unowned1, f1_c1, f1_c2, unowned2, f2_c1}),
      UnorderedElementsAre(
          Pair(blink::WebFormElement(), ElementsAre(unowned1, unowned2)),
          Pair(form1, ElementsAre(f1_c1, f1_c2)),
          Pair(form2, ElementsAre(f2_c1))));
}

}  // namespace
}  // namespace autofill
