// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_agent.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

// A script whose manager fills the custom select with the id `state`:
// `extractOptions()` opens its popover, `closePopover()` closes it, and
// `setValue()` writes the selected option into the element.
constexpr char kFillingScript[] = R"(
  function createCustomFormControlManager() {
    return {
      async extractOptions(node) {
        node.dataset.popover = 'open';
        return {
          options: [{text: 'Bavaria', value: 'BY'}, {text: 'Berlin'}],
          popoverNeedsToBeClosed: true,
        };
      },
      async closePopover(node) {
        node.dataset.popover = 'closed';
      },
      async setValue(node, value, text) {
        if (value === 'missing') {
          return {status: 'OPTION_NOT_FOUND'};
        }
        node.textContent = `${value}:${text}`;
        node.dataset.popover = 'closed';
        return {status: 'OK'};
      },
    };
  }
)";

// A script whose manager detaches its own frame in each fill method, and then
// replies as if it succeeded.
constexpr char kDetachingFillingScript[] = R"(
  function createCustomFormControlManager() {
    return {
      extractOptions(node) {
        frameElement.remove();
        return {options: [{text: 'Bavaria', value: 'BY'}],
                popoverNeedsToBeClosed: true};
      },
      closePopover(node) {
        frameElement.remove();
      },
      setValue(node, value, text) {
        frameElement.remove();
        return {status: 'OK'};
      },
    };
  }
)";

// Like `kDetachingFillingScript`, but each fill method detaches its frame after
// an `await`, i.e. while the call waits for the method's promise.
constexpr char kDetachingAfterAwaitFillingScript[] = R"(
  function createCustomFormControlManager() {
    const detach = async () => {
      await new Promise((resolve) => setTimeout(resolve, 0));
      frameElement.remove();
    };
    return {
      async extractOptions(node) {
        await detach();
        return {options: [{text: 'Bavaria', value: 'BY'}],
                popoverNeedsToBeClosed: true};
      },
      async closePopover(node) {
        await detach();
      },
      async setValue(node, value, text) {
        await detach();
        return {status: 'OK'};
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

  // Extracts the options of `field` in `document` through `remote` and returns
  // the reply.
  mojom::ExtractedCustomFormControlOptionsPtr ExtractOptions(
      Remote& remote,
      const blink::DocumentToken& document,
      FieldRendererId field) {
    base::test::TestFuture<mojom::ExtractedCustomFormControlOptionsPtr>
        extracted;
    remote->ExtractOptions(document, field, extracted.GetCallback());
    return extracted.Take();
  }

  // Closes the popover of `field` in `document` through `remote`, and waits
  // for the reply.
  void ClosePopover(Remote& remote,
                    const blink::DocumentToken& document,
                    FieldRendererId field) {
    base::test::TestFuture<void> closed;
    remote->ClosePopover(document, field, closed.GetCallback());
    EXPECT_TRUE(closed.Wait());
  }

  // Selects the option with `value` and `text` in `field` of `document`
  // through `remote`, and waits for the reply.
  void SetValue(Remote& remote,
                const blink::DocumentToken& document,
                FieldRendererId field,
                std::u16string value,
                std::u16string text) {
    base::test::TestFuture<void> set;
    remote->SetValue(
        document, field,
        mojom::SelectOption::New(std::move(value), std::move(text)),
        set.GetCallback());
    EXPECT_TRUE(set.Wait());
  }

  // Returns the text content of the element with the id `id`.
  std::string GetTextOf(std::string_view id) {
    return GetMainFrame()
        ->GetDocument()
        .GetElementById(blink::WebString::FromUtf8(id))
        .TextContent()
        .Utf8();
  }

  // Returns the value of the attribute `name` of the element with the id `id`.
  std::string GetAttributeOf(std::string_view id, std::string_view name) {
    return GetMainFrame()
        ->GetDocument()
        .GetElementById(blink::WebString::FromUtf8(id))
        .GetAttribute(blink::WebString::FromUtf8(name))
        .Utf8();
  }

  // Creates an `AutofillAgent` with custom form control scripts for the first
  // child frame, which must exist, loads `script` into it, and binds `remote`
  // to its `CustomFormControlAgent`. Returns the child frame's document, or
  // nullopt if that fails. The `AutofillAgent` deletes itself when the frame is
  // destroyed.
  std::optional<blink::DocumentToken> SetUpChildFrameAgent(
      Remote& remote,
      blink::AssociatedInterfaceRegistry& child_registry,
      const std::string& script) {
    blink::WebLocalFrame* child_frame =
        GetMainFrame()->FirstChild()->ToWebLocalFrame();
    content::RenderFrame* child =
        content::RenderFrame::FromWebFrame(child_frame);
    CreateAutofillAgent(child, /*password_autofill_agent=*/nullptr,
                        /*password_generation_agent=*/nullptr, &child_registry)
        .release()
        ->EnableCustomFormControlScripts(kWorldId, child_registry);
    const blink::DocumentToken document = child_frame->GetDocument().Token();
    if (!Bind(remote, child_registry) ||
        !LoadUnsafeScript(remote, document, script)) {
      return std::nullopt;
    }
    return document;
  }

  // Returns the `FieldRendererId` of the element with the id `id` in the first
  // child frame.
  FieldRendererId GetChildFieldRendererIdOf(std::string_view id) {
    return form_util::GetFieldRendererId(
        GetMainFrame()
            ->FirstChild()
            ->ToWebLocalFrame()
            ->GetDocument()
            .GetElementById(blink::WebString::FromUtf8(id)));
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

// Tests that the browser can extract the options of a custom select through
// the interface.
TEST_F(CustomFormControlAgentTest, ExtractsCustomFormControlOptions) {
  LoadHTML("<div id=state></div>");
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kFillingScript));

  mojom::ExtractedCustomFormControlOptionsPtr extracted =
      ExtractOptions(remote, GetDocumentToken(), GetFieldRendererIdOf("state"));
  ASSERT_TRUE(extracted);
  ASSERT_EQ(extracted->options.size(), 2u);
  EXPECT_EQ(extracted->options[0]->value, u"BY");
  EXPECT_EQ(extracted->options[0]->text, u"Bavaria");
  EXPECT_EQ(extracted->options[1]->value, u"Berlin");
  EXPECT_EQ(extracted->options[1]->text, u"Berlin");
  EXPECT_TRUE(extracted->popover_needs_to_be_closed);
  EXPECT_EQ(GetAttributeOf("state", "data-popover"), "open");
}

// Tests that no options are sent if the script's options are invalid (see
// `IsValidOptionVector()`), e.g. too long.
TEST_F(CustomFormControlAgentTest, DropsInvalidOptions) {
  LoadHTML("<div id=state></div>");
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), R"(
    function createCustomFormControlManager() {
      return {
        async extractOptions(node) {
          return {options: [{text: 'x'.repeat(2000)}]};
        },
      };
    }
  )"));
  EXPECT_FALSE(ExtractOptions(remote, GetDocumentToken(),
                              GetFieldRendererIdOf("state")));
}

// Tests that the browser can close the popover of a custom select through the
// interface.
TEST_F(CustomFormControlAgentTest, ClosesCustomFormControlPopover) {
  LoadHTML("<div id=state></div>");
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kFillingScript));

  ClosePopover(remote, GetDocumentToken(), GetFieldRendererIdOf("state"));
  EXPECT_EQ(GetAttributeOf("state", "data-popover"), "closed");
}

// Tests that the browser can select an option of a custom select through the
// interface.
TEST_F(CustomFormControlAgentTest, SetsCustomFormControlValue) {
  LoadHTML("<div id=state></div>");
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kFillingScript));
  const FieldRendererId state = GetFieldRendererIdOf("state");

  SetValue(remote, GetDocumentToken(), state, u"BY", u"Bavaria");
  EXPECT_EQ(GetAttributeOf("state", "data-popover"), "closed");
  EXPECT_EQ(GetTextOf("state"), "BY:Bavaria");
  // A missing option doesn't change the value.
  SetValue(remote, GetDocumentToken(), state, u"missing", u"Missing");
  EXPECT_EQ(GetTextOf("state"), "BY:Bavaria");
}

// Tests that the fill methods fail if they don't name an element of the current
// document.
TEST_F(CustomFormControlAgentTest, FillMethodsFailForInvalidTargets) {
  Remote remote;
  ASSERT_TRUE(Bind(remote));
  const blink::DocumentToken previous_document = GetDocumentToken();
  LoadHTML(
      "<div id=state></div><div id=removed></div>"
      "<iframe srcdoc='<div id=child></div>'>");
  ASSERT_NE(GetDocumentToken(), previous_document);
  ASSERT_TRUE(LoadUnsafeScript(remote, GetDocumentToken(), kFillingScript));
  const FieldRendererId state = GetFieldRendererIdOf("state");
  const FieldRendererId child = GetChildFieldRendererIdOf("child");
  ASSERT_TRUE(child);
  const FieldRendererId removed = GetFieldRendererIdOf("removed");
  ExecuteJavaScriptForTests("document.getElementById('removed').remove();");

  auto expect_fill_methods_fail = [this, &remote](blink::DocumentToken document,
                                                  FieldRendererId field) {
    EXPECT_FALSE(ExtractOptions(remote, document, field));
    ClosePopover(remote, document, field);
    SetValue(remote, document, field, u"BY", u"Bavaria");
  };
  // The previous document, which the browser may name if it sends a call
  // before it learns that the frame committed a new one.
  expect_fill_methods_fail(previous_document, state);
  // A null renderer id.
  expect_fill_methods_fail(GetDocumentToken(), FieldRendererId());
  // A renderer id that no element has.
  expect_fill_methods_fail(GetDocumentToken(), FieldRendererId(*state + 1000));
  // An element of another document (here, of a child frame).
  expect_fill_methods_fail(GetDocumentToken(), child);
  // An element that was removed from the document, so scripts can't access it
  // (see `form_util::IsAccessible()`).
  expect_fill_methods_fail(GetDocumentToken(), removed);
  // Nothing was opened, closed or set.
  EXPECT_EQ(GetAttributeOf("state", "data-popover"), "");
  EXPECT_EQ(GetTextOf("state"), "");
}

// Tests that the reply to `ExtractOptions()` is dropped if `extractOptions()`
// detaches its own frame.
TEST_F(CustomFormControlAgentTest,
       DropsExtractOptionsReplyIfScriptDetachesFrame) {
  LoadHTML("<iframe srcdoc='<div id=state></div>'></iframe>");
  Remote remote;
  blink::AssociatedInterfaceRegistry child_registry;
  std::optional<blink::DocumentToken> document =
      SetUpChildFrameAgent(remote, child_registry, kDetachingFillingScript);
  ASSERT_TRUE(document);

  base::test::TestFuture<mojom::ExtractedCustomFormControlOptionsPtr> extracted;
  remote->ExtractOptions(*document, GetChildFieldRendererIdOf("state"),
                         extracted.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&remote] { return !remote.is_connected(); }));
  EXPECT_FALSE(extracted.IsReady());
}

// Tests that the reply to `ClosePopover()` is dropped if `closePopover()`
// detaches its own frame.
TEST_F(CustomFormControlAgentTest,
       DropsClosePopoverReplyIfScriptDetachesFrame) {
  LoadHTML("<iframe srcdoc='<div id=state></div>'></iframe>");
  Remote remote;
  blink::AssociatedInterfaceRegistry child_registry;
  std::optional<blink::DocumentToken> document =
      SetUpChildFrameAgent(remote, child_registry, kDetachingFillingScript);
  ASSERT_TRUE(document);

  base::test::TestFuture<void> closed;
  remote->ClosePopover(*document, GetChildFieldRendererIdOf("state"),
                       closed.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&remote] { return !remote.is_connected(); }));
  EXPECT_FALSE(closed.IsReady());
}

// Tests that the reply to `SetValue()` is dropped if `setValue()` detaches its
// own frame.
TEST_F(CustomFormControlAgentTest, DropsSetValueReplyIfScriptDetachesFrame) {
  LoadHTML("<iframe srcdoc='<div id=state></div>'></iframe>");
  Remote remote;
  blink::AssociatedInterfaceRegistry child_registry;
  std::optional<blink::DocumentToken> document =
      SetUpChildFrameAgent(remote, child_registry, kDetachingFillingScript);
  ASSERT_TRUE(document);

  base::test::TestFuture<void> result;
  remote->SetValue(*document, GetChildFieldRendererIdOf("state"),
                   mojom::SelectOption::New(u"BY", u"Bavaria"),
                   result.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&remote] { return !remote.is_connected(); }));
  EXPECT_FALSE(result.IsReady());
}

// Tests that the fill methods fail if the script detaches its own frame while
// the call waits for the method's promise: then the executor fails the call
// when the script context is released, before the agent is destroyed.
TEST_F(CustomFormControlAgentTest,
       FillMethodsFailIfScriptDetachesFrameAfterAwait) {
  LoadHTML("<iframe srcdoc='<div id=state></div>'></iframe>");
  Remote remote;
  blink::AssociatedInterfaceRegistry child_registry;
  std::optional<blink::DocumentToken> document = SetUpChildFrameAgent(
      remote, child_registry, kDetachingAfterAwaitFillingScript);
  ASSERT_TRUE(document);

  base::test::TestFuture<mojom::ExtractedCustomFormControlOptionsPtr> extracted;
  remote->ExtractOptions(*document, GetChildFieldRendererIdOf("state"),
                         extracted.GetCallback());
  ASSERT_TRUE(extracted.Wait());
  EXPECT_FALSE(extracted.Get());
}

}  // namespace
}  // namespace autofill
