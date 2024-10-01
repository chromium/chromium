// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_RENDERER_TEST_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_RENDERER_TEST_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "content/public/test/render_view_test.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/web/web_frame_widget.h"

namespace autofill::test {

class MockAutofillDriver : public mojom::AutofillDriver {
 public:
  MockAutofillDriver();
  ~MockAutofillDriver() override;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this, mojo::PendingAssociatedReceiver<mojom::AutofillDriver>(
                             std::move(handle)));
  }

  MOCK_METHOD(void,
              FormsSeen,
              (const std::vector<FormData>& updated_forms,
               const std::vector<FormRendererId>& removed_forms),
              (override));
  MOCK_METHOD(void,
              FormSubmitted,
              (const FormData& form,
               bool known_success,
               mojom::SubmissionSource source),
              (override));
  MOCK_METHOD(void,
              CaretMovedInFormField,
              (const FormData& form,
               FieldRendererId field_id,
               const gfx::Rect& caret_bounds),
              (override));
  MOCK_METHOD(void,
              TextFieldDidChange,
              (const FormData& form,
               FieldRendererId field_id,
               base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void,
              TextFieldDidScroll,
              (const FormData& form, FieldRendererId field_id),
              (override));
  MOCK_METHOD(void,
              SelectControlDidChange,
              (const FormData& form, FieldRendererId field_id),
              (override));
  MOCK_METHOD(void,
              SelectFieldOptionsDidChange,
              (const FormData& form),
              (override));
  MOCK_METHOD(void,
              JavaScriptChangedAutofilledValue,
              (const FormData& form,
               FieldRendererId field_id,
               const std::u16string& old_value,
               bool formatting_ony),
              (override));
  MOCK_METHOD(void,
              AskForValuesToFill,
              (const FormData& form,
               FieldRendererId field_id,
               const gfx::Rect& caret_bounds,
               AutofillSuggestionTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void, HidePopup, (), (override));
  MOCK_METHOD(void, FocusOnNonFormField, (), (override));
  MOCK_METHOD(void,
              FocusOnFormField,
              (const FormData& form, FieldRendererId field_id),
              (override));
  MOCK_METHOD(void,
              DidFillAutofillFormData,
              (const FormData& form, base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void, DidEndTextFieldEditing, (), (override));

 private:
  mojo::AssociatedReceiverSet<mojom::AutofillDriver> receivers_;
};

class AutofillRendererTest : public content::RenderViewTest {
 public:
  AutofillRendererTest();
  ~AutofillRendererTest() override;

  // content::RenderViewTest:
  void SetUp() override;
  void TearDown() override;

  virtual std::unique_ptr<AutofillAgent> CreateAutofillAgent(
      content::RenderFrame* render_frame,
      const AutofillAgent::Config& config,
      std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
      std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
      blink::AssociatedInterfaceRegistry* associated_interfaces);

  blink::WebDocument GetDocument() { return GetMainFrame()->GetDocument(); }

  blink::WebElement GetWebElementById(std::string_view id) {
    return GetDocument().GetElementById(blink::WebString::FromUTF8(id));
  }

  blink::WebFormControlElement GetFormControlElementById(std::string_view id) {
    return GetWebElementById(id).DynamicTo<blink::WebFormControlElement>();
  }

  blink::WebInputElement GetInputElementById(std::string_view id) {
    return GetWebElementById(id).DynamicTo<blink::WebInputElement>();
  }

  // Simulates a click on the element with id `element_id` and, if, successful,
  // runs until the task environment is idle. Waits until the `TaskEnvironment`
  // is idle to ensure that the `AutofillDriver` is notified via mojo.
  bool SimulateElementClickAndWait(const std::string& element_id);

  // Simulates focusing an element without clicking it. Waits until the
  // `TaskEnvironment` is idle to ensure that the `AutofillDriver` is notified
  // via mojo.
  void SimulateElementFocusAndWait(std::string_view element_id);

  // Simulates scrolling. Waits until the `TaskEnvironment` is idle to ensure
  // that the `AutofillDriver` is notified via mojo.
  void SimulateScrollingAndWait();

  // AutofillDriver::FormsSeen() is throttled indirectly because some callsites
  // of AutofillAgent::ProcessForms() are throttled. This function blocks until
  // FormsSeen() has happened.
  void WaitForFormsSeen() {
    task_environment_.FastForwardBy(AutofillAgent::kFormsSeenThrottle * 3 / 2);
  }

  // This triggers a layout update to apply JS changes like display = 'none'.
  void ForceLayoutUpdate() {
    GetWebFrameWidget()->UpdateAllLifecyclePhases(
        blink::DocumentUpdateReason::kTest);
  }

 protected:
  AutofillAgent& autofill_agent() { return *autofill_agent_; }
  MockAutofillDriver& autofill_driver() { return autofill_driver_; }

 private:
  ::testing::NiceMock<MockAutofillDriver> autofill_driver_;
  blink::AssociatedInterfaceRegistry associated_interfaces_;
  std::unique_ptr<AutofillAgent> autofill_agent_;
};

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_RENDERER_TEST_H_
