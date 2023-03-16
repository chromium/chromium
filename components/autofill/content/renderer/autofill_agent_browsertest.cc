// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::SizeIs;

namespace autofill {

namespace {

class MockAutofillDriver : public mojom::AutofillDriver {
 public:
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this, mojo::PendingAssociatedReceiver<mojom::AutofillDriver>(
                             std::move(handle)));
  }

  MOCK_METHOD(void,
              SetFormToBeProbablySubmitted,
              (const absl::optional<FormData>& form),
              (override));
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
              TextFieldDidChange,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box,
               base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void,
              TextFieldDidScroll,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              SelectControlDidChange,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              SelectFieldOptionsDidChange,
              (const FormData& form),
              (override));
  MOCK_METHOD(void,
              JavaScriptChangedAutofilledValue,
              (const FormData& form,
               const FormFieldData& field,
               const std::u16string& old_value),
              (override));
  MOCK_METHOD(void,
              AskForValuesToFill,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box,
               AutoselectFirstSuggestion autoselect_first_suggestion,
               FormElementWasClicked form_element_was_clicked),
              (override));
  MOCK_METHOD(void, HidePopup, (), (override));
  MOCK_METHOD(void,
              FocusNoLongerOnForm,
              (bool had_interacted_form),
              (override));
  MOCK_METHOD(void,
              FocusOnFormField,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              DidFillAutofillFormData,
              (const FormData& form, base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void, DidPreviewAutofillFormData, (), (override));
  MOCK_METHOD(void, DidEndTextFieldEditing, (), (override));

 private:
  mojo::AssociatedReceiverSet<mojom::AutofillDriver> receivers_;
};

// Matches a specific FormRendererId.
auto IsFormId(absl::variant<FormRendererId, size_t> expectation) {
  FormRendererId id = absl::holds_alternative<FormRendererId>(expectation)
                          ? absl::get<FormRendererId>(expectation)
                          : FormRendererId(absl::get<size_t>(expectation));
  return Eq(id);
}

// Matches a FormData with a specific FormData::unique_renderer_id.
auto HasFormId(absl::variant<FormRendererId, size_t> expectation) {
  return Field(&FormData::unique_renderer_id, IsFormId(expectation));
}

// Matches a FormData with |num| FormData::fields.
auto HasNumFields(size_t num) {
  return Field(&FormData::fields, SizeIs(num));
}

// Matches a FormData with |num| FormData::child_frames.
auto HasNumChildFrames(size_t num) {
  return Field(&FormData::child_frames, SizeIs(num));
}

// Matches a container with a single element which (the element) matches all
// |element_matchers|.
template <typename... Matchers>
auto HasSingleElementWhich(Matchers... element_matchers) {
  return AllOf(SizeIs(1), ElementsAre(AllOf(element_matchers...)));
}

}  // namespace

// TODO(crbug.com/63573): Add many more test cases.
class AutofillAgentTest : public content::RenderViewTest {
 public:
  void SetUp() override {
    RenderViewTest::SetUp();

    blink::AssociatedInterfaceProvider* remote_interfaces =
        GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillDriver::Name_,
        base::BindRepeating(&MockAutofillDriver::BindPendingReceiver,
                            base::Unretained(&autofill_driver_)));

    password_autofill_agent_ = std::make_unique<TestPasswordAutofillAgent>(
        GetMainRenderFrame(), &associated_interfaces_);
    password_generation_ = std::make_unique<PasswordGenerationAgent>(
        GetMainRenderFrame(), password_autofill_agent_.get(),
        &associated_interfaces_);
    autofill_agent_ = std::make_unique<AutofillAgent>(
        GetMainRenderFrame(), password_autofill_agent_.get(),
        password_generation_.get(), &associated_interfaces_);
  }

  void TearDown() override {
    autofill_agent_.reset();
    password_generation_.reset();
    password_autofill_agent_.reset();
    RenderViewTest::TearDown();
  }

  AutofillAgentTestApi test_api() {
    return AutofillAgentTestApi(autofill_agent_.get());
  }

 protected:
  MockAutofillDriver autofill_driver_;
  std::unique_ptr<AutofillAgent> autofill_agent_;

 private:
  blink::AssociatedInterfaceRegistry associated_interfaces_;
  std::unique_ptr<PasswordAutofillAgent> password_autofill_agent_;
  std::unique_ptr<PasswordGenerationAgent> password_generation_;
};

// Enables AutofillAcrossIframes.
class AutofillAgentTestWithFeatures : public AutofillAgentTest {
 public:
  AutofillAgentTestWithFeatures() {
    scoped_features_.InitAndEnableFeature(features::kAutofillAcrossIframes);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_Empty) {
  EXPECT_CALL(autofill_driver_, FormsSeen).Times(0);
  LoadHTML(R"(<body> </body>)");
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NoEmpty) {
  EXPECT_CALL(autofill_driver_, FormsSeen).Times(0);
  LoadHTML(R"(<body> <form></form> </body>)");
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewFormUnowned) {
  EXPECT_CALL(autofill_driver_,
              FormsSeen(HasSingleElementWhich(HasFormId(0), HasNumFields(1),
                                              HasNumChildFrames(0)),
                        SizeIs(0)));
  LoadHTML(R"(<body> <input> </body>)");
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewForm) {
  EXPECT_CALL(autofill_driver_,
              FormsSeen(HasSingleElementWhich(HasFormId(1), HasNumFields(1),
                                              HasNumChildFrames(0)),
                        SizeIs(0)));
  LoadHTML(R"(<body> <form><input></form> </body>)");
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewIframe) {
  EXPECT_CALL(autofill_driver_,
              FormsSeen(HasSingleElementWhich(HasFormId(1), HasNumFields(0),
                                              HasNumChildFrames(1)),
                        SizeIs(0)));
  LoadHTML(R"(<body> <form><iframe></iframe></form> </body>)");
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_UpdatedForm) {
  {
    EXPECT_CALL(autofill_driver_,
                FormsSeen(HasSingleElementWhich(HasFormId(1), HasNumFields(1),
                                                HasNumChildFrames(0)),
                          SizeIs(0)));
    LoadHTML(R"(<body> <form><input></form> </body>)");
  }
  {
    EXPECT_CALL(autofill_driver_,
                FormsSeen(HasSingleElementWhich(HasFormId(1), HasNumFields(2),
                                                HasNumChildFrames(0)),
                          SizeIs(0)));
    ExecuteJavaScriptForTests(
        R"(document.forms[0].appendChild(document.createElement('input'));)");
    content::RunAllTasksUntilIdle();
    // Called explicitly because the event is throttled.
    test_api().DidAddOrRemoveFormRelatedElementsDynamically();
  }
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_RemovedForm) {
  {
    EXPECT_CALL(autofill_driver_, FormsSeen(SizeIs(1), SizeIs(0)));
    LoadHTML(R"(<body> <form><input></form> </body>)");
  }
  {
    EXPECT_CALL(autofill_driver_,
                FormsSeen(SizeIs(0), HasSingleElementWhich(IsFormId(1))));
    ExecuteJavaScriptForTests(R"(document.forms[0].remove();)");
    content::RunAllTasksUntilIdle();
    // Called explicitly because the event is throttled.
    test_api().DidAddOrRemoveFormRelatedElementsDynamically();
  }
}

TEST_F(AutofillAgentTestWithFeatures, TriggerReparseWithResponse) {
  base::MockOnceCallback<void(bool)> mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  autofill_agent_->TriggerReparseWithResponse(mock_callback.Get());
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_CALL(mock_callback, Run(true));
  task_environment_.FastForwardBy(base::Milliseconds(50));
}

TEST_F(AutofillAgentTestWithFeatures, TriggerReparseWithResponse_CalledTwice) {
  base::MockOnceCallback<void(bool)> mock_callback;
  autofill_agent_->TriggerReparseWithResponse(mock_callback.Get());
  EXPECT_CALL(mock_callback, Run(false));
  autofill_agent_->TriggerReparseWithResponse(mock_callback.Get());
}

}  // namespace autofill
