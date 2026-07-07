// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_renderer_test.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/map_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill::test {

using ::base::test::RunClosure;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Property;

MockAutofillDriver::MockAutofillDriver() = default;

MockAutofillDriver::~MockAutofillDriver() = default;

AutofillRendererTest::AutofillRendererTest() = default;

AutofillRendererTest::~AutofillRendererTest() = default;

void AutofillRendererTest::SetUp() {
  RenderViewTest::SetUp();

  blink::AssociatedInterfaceProvider* remote_interfaces =
      GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::AutofillDriver::Name_,
      base::BindRepeating(&MockAutofillDriver::BindPendingReceiver,
                          base::Unretained(&autofill_driver_)));

  auto password_autofill_agent = std::make_unique<TestPasswordAutofillAgent>(
      GetMainRenderFrame(), &associated_interfaces_);
  auto password_generation_agent = std::make_unique<PasswordGenerationAgent>(
      GetMainRenderFrame(), password_autofill_agent.get(),
      &associated_interfaces_);
  autofill_agent_ = CreateAutofillAgent(
      GetMainRenderFrame(), std::move(password_autofill_agent),
      std::move(password_generation_agent), &associated_interfaces_);
}

void AutofillRendererTest::TearDown() {
  // Explicitly set the `AutofillClient` to null before resetting the agent -
  // otherwise the frame has a dangling pointer and document unloading may
  // cause a UAF.
  GetMainFrame()->SetAutofillClient(nullptr);
  autofill_agent_.reset();
  RenderViewTest::TearDown();
}

std::unique_ptr<AutofillAgent> AutofillRendererTest::CreateAutofillAgent(
    content::RenderFrame* render_frame,
    std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
    std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  return std::make_unique<AutofillAgent>(
      render_frame, std::move(password_autofill_agent),
      std::move(password_generation_agent), associated_interfaces);
}

std::optional<FormData> AutofillRendererTest::ExtractFormData(
    blink::WebFormElement form_element) {
  constexpr CallTimerState kCallTimerStateDummy = {
      .call_site = CallTimerState::CallSite::kUpdateFormCache,
      .last_autofill_agent_reset = {},
      .last_dom_content_loaded = {},
  };

  return form_util::ExtractFormData(GetDocument(), form_element,
                                    autofill_agent_->field_data_manager(),
                                    kCallTimerStateDummy,
                                    /*button_titles_cache=*/nullptr);
}

std::optional<FormData> AutofillRendererTest::ExtractFormData(
    std::string_view form_id) {
  return ExtractFormData(
      GetWebElementById(form_id).To<blink::WebFormElement>());
}

AssertionResult AutofillRendererTest::SimulateFillForm(
    const FormData& form_data,
    std::string_view initiate_click_element_id,
    const base::flat_map<std::u16string, std::u16string>& fill_values_by_id) {
  blink::WebFormControlElement element =
      GetWebElementById(initiate_click_element_id)
          .To<blink::WebFormControlElement>();

  if (!element) {
    return AssertionFailure();
  }
  // This click simulation is necessary to set up the autofill agent
  // appropriately for the user selection; it simulates the menu actually
  // popping up.
  SimulatePointClick(element.BoundsInWidget().CenterPoint());

  std::vector<FormFieldData::FillData> fields_for_filling;
  fields_for_filling.reserve(form_data.fields().size());
  for (const FormFieldData& field : form_data.fields()) {
    if (const std::u16string* fill_value =
            base::FindOrNull(fill_values_by_id, field.id_attribute())) {
      FormFieldData::FillData fill_data(field);
      fill_data.value = *fill_value;
      fill_data.is_autofilled = true;
      fields_for_filling.push_back(std::move(fill_data));
    }
  }

  bool success = false;
  base::RunLoop run_loop;
  ON_CALL(autofill_driver(), DidAutofillForm(Property(&FormData::global_id,
                                                      form_data.global_id())))
      .WillByDefault(
          DoAll(RunClosure(run_loop.QuitClosure()), Assign(&success, true)));

  autofill_agent().ApplyFieldsAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kFill,
                                     fields_for_filling, FillId::Create(),
                                     /*supports_refill=*/false);
  std::move(run_loop).Run();
  return AssertionResult(success);
}

bool AutofillRendererTest::SimulateElementClickAndWait(
    const std::string& element_id) {
  if (!SimulateElementClick(element_id)) {
    return false;
  }
  task_environment_.RunUntilIdle();
  return true;
}

void AutofillRendererTest::SimulateElementFocusAndWait(
    std::string_view element_id) {
  ExecuteJavaScriptForTests(
      base::StrCat({"document.getElementById('", element_id, "').focus();"}));
  task_environment_.RunUntilIdle();
}

void AutofillRendererTest::Focus(std::string_view element_id) {
  ExecuteJavaScriptForTests(
      base::StrCat({"document.getElementById('", element_id, "').focus();"}));
  task_environment_.FastForwardBy(base::Milliseconds(500));
  task_environment_.RunUntilIdle();
}

void AutofillRendererTest::SimulateScrollingAndWait() {
  ExecuteJavaScriptForTests("window.scrollTo(0, 1000);");
  task_environment_.RunUntilIdle();
}

}  // namespace autofill::test
