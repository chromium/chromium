// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_agent.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Property;

// A minimal script that returns an empty manager.
constexpr char kValidScript[] =
    "function createCustomFormControlManager() { return {}; }";

// Any isolated world id that the test doesn't otherwise use.
constexpr int32_t kWorldId = content::ISOLATED_WORLD_ID_CONTENT_END + 1;

// A script whose manager detects the elements with the ids `state` and
// `country` as custom selects. Only `state` is active.
constexpr char kDetectingScript[] = R"(
  function createCustomFormControlManager() {
    const entry = (id, fieldType) => ({
      canonicalDomNode: document.getElementById(id),
      controlType: 'select',
      fieldType,
      isActive: id === 'state',
    });
    return {
      detectCustomFormControls() {
        return [
          entry('state', 'ADDRESS_HOME_STATE'),
          entry('country', 'ADDRESS_HOME_COUNTRY'),
        ];
      },
    };
  }
)";

using Remote = mojo::AssociatedRemote<mojom::CustomFormControlAgent>;

class CustomFormControlAgentTest : public test::AutofillRendererTest {
 protected:
  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    autofill_agent().EnableCustomFormControlScripts(kWorldId, registry_);
  }

  // Binds `remote` to the agent through `registry`, as the browser does.
  // Returns false if that fails.
  [[nodiscard]] static bool Bind(Remote& remote,
                                 blink::AssociatedInterfaceRegistry& registry) {
    mojo::ScopedInterfaceEndpointHandle handle =
        remote.BindNewEndpointAndPassDedicatedReceiver().PassHandle();
    return registry.TryBindInterface(mojom::CustomFormControlAgent::Name_,
                                     &handle);
  }
  [[nodiscard]] bool Bind(Remote& remote) { return Bind(remote, registry_); }

  // Loads `script` for `document` through `remote` and returns the reply.
  bool LoadUnsafeScript(Remote& remote,
                        const blink::DocumentToken& document,
                        const std::string& script) {
    base::test::TestFuture<bool> loaded;
    remote->LoadUnsafeScript(document, script, loaded.GetCallback());
    return loaded.Get();
  }

  // Detects the custom selects of `document` through `remote` and returns the
  // reply.
  std::vector<mojom::DetectedCustomFormControlPtr> DetectCustomFormControls(
      Remote& remote,
      const blink::DocumentToken& document) {
    base::test::TestFuture<std::vector<mojom::DetectedCustomFormControlPtr>>
        detected;
    remote->DetectCustomFormControls(document, detected.GetCallback());
    return detected.Take();
  }

  // Returns the `FieldRendererId` of the element with the id `id`.
  FieldRendererId GetFieldRendererIdOf(std::string_view id) {
    return form_util::GetFieldRendererId(
        GetMainFrame()->GetDocument().GetElementById(
            blink::WebString::FromUtf8(id)));
  }

  blink::DocumentToken GetDocumentToken() {
    return GetMainFrame()->GetDocument().Token();
  }

  CustomFormControlScriptExecutor& executor() {
    return CHECK_DEREF(autofill_agent().custom_form_control_script_executor());
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillCustomFormControlScripts};
  blink::AssociatedInterfaceRegistry registry_;
};

// Tests that the browser can load scripts through the interface and learns
// whether they loaded.
TEST_F(CustomFormControlAgentTest, LoadsScripts) {
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  EXPECT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kValidScript));
  EXPECT_TRUE(executor().has_manager());

  EXPECT_FALSE(
      LoadUnsafeScript(remote, GetDocumentToken(), "throw new Error();"));
  EXPECT_FALSE(executor().has_manager());
}

// Tests that a script for a previous document, which the browser sent before
// the frame committed a new one, isn't loaded.
TEST_F(CustomFormControlAgentTest, IgnoresScriptForPreviousDocument) {
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  const blink::DocumentToken previous_document = GetDocumentToken();
  LoadHTML("<div>New document</div>");
  ASSERT_NE(GetDocumentToken(), previous_document);

  EXPECT_FALSE(LoadUnsafeScript(remote, previous_document, kValidScript));
  EXPECT_FALSE(executor().has_manager());
  // The connection still works for the current document.
  EXPECT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kValidScript));
}

// Tests that binding the interface again replaces the previous connection.
TEST_F(CustomFormControlAgentTest, RebindReplacesConnection) {
  Remote first;
  ASSERT_TRUE(Bind(first));
  Remote second;
  ASSERT_TRUE(Bind(second));
  EXPECT_TRUE(base::test::RunUntil([&] { return !first.is_connected(); }));
  EXPECT_TRUE(LoadUnsafeScript(second, GetDocumentToken(), kValidScript));
}

// Tests that loading a valid script triggers form re-extraction when
// `features::kAutofillCustomFormControlExtraction` is enabled.
TEST_F(CustomFormControlAgentTest, TriggersFormExtraction) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillCustomFormControlExtraction};
  LoadHTML(R"(<form id="f"><div id="custom"></div><input id="input"></form>)");
  WaitForFormsSeen();

  Remote remote;
  ASSERT_TRUE(Bind(remote));
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(
          ElementsAre(Property(
              &FormData::fields,
              ElementsAre(Property(&FormFieldData::id_attribute, u"custom"),
                          Property(&FormFieldData::id_attribute, u"input")))),
          _));
  EXPECT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), R"(
    function createCustomFormControlManager() {
      return {
        detectCustomFormControls: () => [{
          canonicalDomNode: document.getElementById("custom"),
          fieldType: "ADDRESS_HOME_COUNTRY",
          isActive: true,
        }],
      };
    }
  )"));
  WaitForFormsSeen();
}

// Tests that the reply is dropped if the script detaches its own frame.
TEST_F(CustomFormControlAgentTest, DropsReplyIfScriptDetachesFrame) {
  LoadHTML("<iframe src='about:blank'></iframe>");
  blink::WebLocalFrame* child_frame =
      GetMainFrame()->FirstChild()->ToWebLocalFrame();
  content::RenderFrame* child = content::RenderFrame::FromWebFrame(child_frame);
  blink::AssociatedInterfaceRegistry child_registry;
  // `AutofillAgent` deletes itself when `child` is destroyed, and calls
  // `CustomFormControlAgent::OnDestruct()` in `AutofillAgent::OnDestruct()`.
  auto* child_autofill_agent =
      CreateAutofillAgent(child, /*password_autofill_agent=*/nullptr,
                          /*password_generation_agent=*/nullptr,
                          &child_registry)
          .release();
  child_autofill_agent->EnableCustomFormControlScripts(kWorldId,
                                                       child_registry);
  Remote remote;
  ASSERT_TRUE(Bind(remote, child_registry));

  static constexpr char kDetachingScript[] = R"(
    frameElement.remove();
    function createCustomFormControlManager() { return {}; }
  )";
  base::test::TestFuture<bool> loaded;
  remote->LoadUnsafeScript(child_frame->GetDocument().Token(), kDetachingScript,
                           loaded.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&remote] { return !remote.is_connected(); }));
  EXPECT_FALSE(loaded.IsReady());
}

// Tests that the browser can detect the custom selects through the interface.
TEST_F(CustomFormControlAgentTest, DetectsCustomFormControls) {
  LoadHTML("<div id=state></div><div id=country></div>");
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  // Nothing is detected without a script.
  EXPECT_TRUE(DetectCustomFormControls(remote, GetDocumentToken()).empty());
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kDetectingScript));

  std::vector<mojom::DetectedCustomFormControlPtr> controls =
      DetectCustomFormControls(remote, GetDocumentToken());
  ASSERT_EQ(controls.size(), 2u);
  EXPECT_EQ(controls[0]->field_id, GetFieldRendererIdOf("state"));
  EXPECT_EQ(controls[0]->field_type, "ADDRESS_HOME_STATE");
  EXPECT_TRUE(controls[0]->is_active);
  EXPECT_EQ(controls[1]->field_id, GetFieldRendererIdOf("country"));
  EXPECT_EQ(controls[1]->field_type, "ADDRESS_HOME_COUNTRY");
  EXPECT_FALSE(controls[1]->is_active);
}

// Tests that the agent sends each element at most once.
TEST_F(CustomFormControlAgentTest, SendsEachElementOnce) {
  LoadHTML("<div id=state></div><div id=city></div>");
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), R"(
    function createCustomFormControlManager() {
      const entry = (id, fieldType) => ({
        canonicalDomNode: document.getElementById(id),
        fieldType,
        isActive: true,
      });
      return {
        detectCustomFormControls: () => [
          entry('state', 'ADDRESS_HOME_STATE'),
          entry('state', 'ADDRESS_HOME_COUNTRY'),
          entry('city', 'ADDRESS_HOME_CITY'),
        ],
      };
    }
  )"));

  std::vector<mojom::DetectedCustomFormControlPtr> controls =
      DetectCustomFormControls(remote, GetDocumentToken());
  ASSERT_EQ(controls.size(), 2u);
  EXPECT_EQ(controls[0]->field_id, GetFieldRendererIdOf("state"));
  EXPECT_EQ(controls[0]->field_type, "ADDRESS_HOME_STATE");
  EXPECT_EQ(controls[1]->field_id, GetFieldRendererIdOf("city"));
  EXPECT_EQ(controls[1]->field_type, "ADDRESS_HOME_CITY");
}

// Tests that the agent sends at most `kMaxExtractableFields` controls.
TEST_F(CustomFormControlAgentTest, LimitsNumberOfDetectedControls) {
  // One more element than the agent may send.
  std::string html;
  for (size_t i = 0; i < kMaxExtractableFields + 1; ++i) {
    html += "<div></div>";
  }
  LoadHTML(html.c_str());
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), R"(
    function createCustomFormControlManager() {
      return {
        detectCustomFormControls: () =>
            Array.from(document.querySelectorAll('div'), (div) => ({
              canonicalDomNode: div,
              fieldType: 'ADDRESS_HOME_STATE',
              isActive: true,
            })),
      };
    }
  )"));
  EXPECT_EQ(DetectCustomFormControls(remote, GetDocumentToken()).size(),
            kMaxExtractableFields);
}

// Tests that the reply is dropped if `detectCustomFormControls()` detaches its
// own frame.
TEST_F(CustomFormControlAgentTest, DropsDetectReplyIfScriptDetachesFrame) {
  LoadHTML("<iframe srcdoc='<div id=state></div>'></iframe>");
  blink::WebLocalFrame* child_frame =
      GetMainFrame()->FirstChild()->ToWebLocalFrame();
  content::RenderFrame* child = content::RenderFrame::FromWebFrame(child_frame);
  blink::AssociatedInterfaceRegistry child_registry;
  // `AutofillAgent` deletes itself when `child` is destroyed.
  auto* child_autofill_agent =
      CreateAutofillAgent(child, /*password_autofill_agent=*/nullptr,
                          /*password_generation_agent=*/nullptr,
                          &child_registry)
          .release();
  child_autofill_agent->EnableCustomFormControlScripts(kWorldId,
                                                       child_registry);
  Remote remote;
  ASSERT_TRUE(Bind(remote, child_registry));
  static constexpr char kDetachingScript[] = R"(
    function createCustomFormControlManager() {
      return {
        detectCustomFormControls() {
          const state = document.getElementById('state');
          frameElement.remove();
          return [{
            canonicalDomNode: state,
            controlType: 'select',
            fieldType: 'ADDRESS_HOME_STATE',
            isActive: true,
            currentValue: null,
          }];
        },
      };
    }
  )";
  ASSERT_TRUE(LoadUnsafeScript(remote, child_frame->GetDocument().Token(),
                               kDetachingScript));

  base::test::TestFuture<std::vector<mojom::DetectedCustomFormControlPtr>>
      detected;
  remote->DetectCustomFormControls(child_frame->GetDocument().Token(),
                                   detected.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&remote] { return !remote.is_connected(); }));
  EXPECT_FALSE(detected.IsReady());
}

// Tests that detection for the previous document, which the browser requested
// before the frame committed a new one, returns no controls.
TEST_F(CustomFormControlAgentTest,
       DetectCustomFormControlsIgnoresPreviousDocument) {
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  const blink::DocumentToken previous_document = GetDocumentToken();
  LoadHTML("<div id=state></div><div id=country></div>");
  ASSERT_NE(GetDocumentToken(), previous_document);
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kDetectingScript));

  EXPECT_TRUE(DetectCustomFormControls(remote, previous_document).empty());
  // Detection works for the current document.
  EXPECT_EQ(DetectCustomFormControls(remote, GetDocumentToken()).size(), 2u);
}

}  // namespace
}  // namespace autofill
