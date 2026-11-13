// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_runner.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/run_until.h"
#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_runner_test_api.h"
#include "components/autofill/content/browser/integrators/custom_form_controls/mock_custom_form_control_agent.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/custom_form_controls/custom_form_control_script_index.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::SizeIs;

// Returns a detected control as the renderer sends it.
mojom::DetectedCustomFormControlPtr MakeControl(uint32_t field_id,
                                                std::string field_type,
                                                bool is_active = true) {
  return mojom::DetectedCustomFormControl::New(
      FieldRendererId(field_id), std::move(field_type), is_active);
}

class CustomFormControlScriptRunnerTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    runner_ =
        std::make_unique<CustomFormControlScriptRunner>(client(), &index_);
    runner_->set_script_found_callback_for_testing(
        base::BindLambdaForTesting([this](const std::string& script) {
          found_scripts_.push_back(script);
        }));
    // By default, scripts load and no controls are detected. Without these
    // replies, the mojo responders would be dropped without running.
    ON_CALL(agent_, LoadUnsafeScript)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<2>(true));
    ON_CALL(agent_, DetectCustomFormControls)
        .WillByDefault(
            [](const blink::DocumentToken&,
               MockCustomFormControlAgent::DetectCustomFormControlsCallback
                   callback) { std::move(callback).Run({}); });
  }

  void TearDown() override {
    runner_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Binds the main frame's requests for the agent to `agent_` and returns it.
  MockCustomFormControlAgent& BindAgent() {
    main_rfh()->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::CustomFormControlAgent::Name_,
        base::BindRepeating(&MockCustomFormControlAgent::BindPendingReceiver,
                            base::Unretained(&agent_)));
    return agent_;
  }

  // Initializes `index_` with a single entry that maps `host` to `script` and
  // waits until it is done. Returns false if that fails.
  [[nodiscard]] bool InitializeIndex(std::string_view host,
                                     std::string_view script) {
    const base::FilePath dir = temp_dir_.GetPath();
    if (!base::WriteFile(
            dir.AppendASCII("index.json"),
            base::StrCat({R"([{"host": ")", host,
                          R"(", "script_path": "script.js"}])"})) ||
        !base::WriteFile(dir.AppendASCII("script.js"), script)) {
      return false;
    }
    index_.InitializeFromDirectory(dir);
    return base::test::RunUntil([&] { return index_.is_initialized(); });
  }

  // Notifies the runner that forms were seen in the main frame.
  void SeeForms() {
    runner().OnAfterFormsSeen(manager(), {test::MakeFormGlobalId()}, {});
  }

  // Like `SeeForms()`, but the main frame's agent detects `controls` in the
  // main frame's document. Waits until some of them are stored. Returns false
  // if that fails.
  [[nodiscard]] bool DetectControls(
      std::vector<mojom::DetectedCustomFormControlPtr> controls) {
    EXPECT_CALL(BindAgent(),
                DetectCustomFormControls(main_rfh()->GetDocumentToken(), _))
        .WillOnce(RunOnceCallback<1>(std::move(controls)));
    SeeForms();
    return base::test::RunUntil(
        [&] { return !test_api(runner()).detected_controls().empty(); });
  }

  // Like `SeeForms()`, but the main frame's agent detects `controls`, which
  // violate the requirements of `mojom::DetectedCustomFormControl`. Returns
  // the bad message that the runner reports.
  std::string DetectAndWaitForBadMessage(
      std::vector<mojom::DetectedCustomFormControlPtr> controls) {
    mojo::test::BadMessageObserver bad_message_observer;
    EXPECT_CALL(BindAgent(), DetectCustomFormControls)
        .WillOnce(RunOnceCallback<1>(std::move(controls)));
    SeeForms();
    return bad_message_observer.WaitForBadMessage();
  }

  CustomFormControlScriptRunner& runner() { return *runner_; }

  TestContentAutofillClient& client() {
    return *autofill_client_injector_[web_contents()];
  }

  TestBrowserAutofillManager& manager() { return manager(main_rfh()); }
  TestBrowserAutofillManager& manager(content::RenderFrameHost* rfh) {
    return *autofill_manager_injector_[rfh];
  }

  const std::vector<std::string>& found_scripts() const {
    return found_scripts_;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  base::ScopedTempDir temp_dir_;
  CustomFormControlScriptIndex index_;
  std::unique_ptr<CustomFormControlScriptRunner> runner_;
  std::vector<std::string> found_scripts_;
  NiceMock<MockCustomFormControlAgent> agent_;
};

// Tests that the script for the main frame's URL is found when forms are seen.
TEST_F(CustomFormControlScriptRunnerTest, FindsScriptWhenFormsAreSeen) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script"));
}

// Tests that each document is only looked up once.
TEST_F(CustomFormControlScriptRunnerTest, LooksUpEachDocumentOnce) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script"));
}

// Tests that the document a navigation commits is looked up again.
TEST_F(CustomFormControlScriptRunnerTest, LooksUpNewDocument) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  NavigateAndCommit(GURL("https://example.com/payment"));
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script", "example script"));
}

// Tests that the main frame's document is looked up again after its manager
// is reset for a new document in the same frame.
TEST_F(CustomFormControlScriptRunnerTest, LooksUpAgainAfterReset) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  runner().OnAutofillManagerStateChanged(
      manager(), AutofillManager::LifecycleState::kActive,
      AutofillManager::LifecycleState::kPendingReset);
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script", "example script"));
}

// Tests that a found script is loaded into the frame's renderer for the
// frame's current document.
TEST_F(CustomFormControlScriptRunnerTest, LoadsScriptIntoRenderer) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  MockCustomFormControlAgent& agent = BindAgent();
  base::RunLoop run_loop;
  EXPECT_CALL(agent, LoadUnsafeScript(main_rfh()->GetDocumentToken(),
                                      "example script", _))
      .WillOnce(
          DoAll(RunClosure(run_loop.QuitClosure()), RunOnceCallback<2>(true)));
  SeeForms();
  run_loop.Run();
}

// Tests that the controls that the script detects are stored after the script
// loaded.
TEST_F(CustomFormControlScriptRunnerTest, DetectsControlsAfterLoadingScript) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  controls.push_back(MakeControl(1, "ADDRESS_HOME_STATE"));
  controls.push_back(MakeControl(2, "ADDRESS_HOME_COUNTRY"));
  ASSERT_TRUE(DetectControls(std::move(controls)));
  EXPECT_THAT(test_api(runner()).detected_controls(),
              ElementsAre(FieldsAre(FieldRendererId(1), ADDRESS_HOME_STATE),
                          FieldsAre(FieldRendererId(2), ADDRESS_HOME_COUNTRY)));
}

// Tests that controls aren't detected if the script fails to load.
TEST_F(CustomFormControlScriptRunnerTest, DoesNotDetectIfScriptFailsToLoad) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  MockCustomFormControlAgent& agent = BindAgent();
  base::OnceCallback<void(bool)> load_reply;
  EXPECT_CALL(agent, LoadUnsafeScript).WillOnce(MoveArg<2>(&load_reply));
  EXPECT_CALL(agent, DetectCustomFormControls).Times(0);
  SeeForms();
  ASSERT_TRUE(base::test::RunUntil([&] { return !load_reply.is_null(); }));
  std::move(load_reply).Run(false);
  agent.Flush();
}

// Tests that detected controls are skipped if their type is unknown or a meta
// type without a group, or if they are inactive.
TEST_F(CustomFormControlScriptRunnerTest, SkipsUnfillableControls) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  controls.push_back(MakeControl(1, "NOT_A_FIELD_TYPE"));
  controls.push_back(MakeControl(2, "ADDRESS_HOME_STATE"));
  controls.push_back(MakeControl(3, "NO_SERVER_DATA"));
  controls.push_back(MakeControl(4, "EMPTY_TYPE"));
  controls.push_back(MakeControl(5, "ADDRESS_HOME_CITY", /*is_active=*/false));
  ASSERT_TRUE(DetectControls(std::move(controls)));
  EXPECT_THAT(test_api(runner()).detected_controls(),
              ElementsAre(FieldsAre(FieldRendererId(2), ADDRESS_HOME_STATE)));
}

// Tests that a control without a `FieldRendererId` is a bad message, and that
// no controls are stored then.
TEST_F(CustomFormControlScriptRunnerTest, ReportsControlWithoutId) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  controls.push_back(MakeControl(1, "ADDRESS_HOME_STATE"));
  controls.push_back(MakeControl(0, "ADDRESS_HOME_COUNTRY"));
  EXPECT_EQ(DetectAndWaitForBadMessage(std::move(controls)),
            "Custom form control without renderer id");
  EXPECT_THAT(test_api(runner()).detected_controls(), IsEmpty());
}

// Tests that two controls with the same `FieldRendererId` are a bad message.
TEST_F(CustomFormControlScriptRunnerTest, ReportsControlsWithDuplicateIds) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  controls.push_back(MakeControl(1, "ADDRESS_HOME_STATE"));
  controls.push_back(MakeControl(1, "ADDRESS_HOME_COUNTRY"));
  EXPECT_EQ(DetectAndWaitForBadMessage(std::move(controls)),
            "Duplicate custom form control renderer id");
  EXPECT_THAT(test_api(runner()).detected_controls(), IsEmpty());
}

// Tests that more than `kMaxExtractableFields` controls are a bad message.
TEST_F(CustomFormControlScriptRunnerTest, ReportsTooManyControls) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  for (uint32_t i = 1; i <= kMaxExtractableFields + 1; ++i) {
    controls.push_back(MakeControl(i, "ADDRESS_HOME_STATE"));
  }
  EXPECT_EQ(DetectAndWaitForBadMessage(std::move(controls)),
            "Too many custom form controls");
  EXPECT_THAT(test_api(runner()).detected_controls(), IsEmpty());
}

// Tests that a reply to `LoadUnsafeScript()` that arrives after the frame's
// manager was reset for a new document doesn't trigger detection.
TEST_F(CustomFormControlScriptRunnerTest, IgnoresLoadReplyAfterReset) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  MockCustomFormControlAgent& agent = BindAgent();
  base::OnceCallback<void(bool)> load_reply;
  EXPECT_CALL(agent, LoadUnsafeScript).WillOnce(MoveArg<2>(&load_reply));
  EXPECT_CALL(agent, DetectCustomFormControls).Times(0);
  SeeForms();
  ASSERT_TRUE(base::test::RunUntil([&] { return !load_reply.is_null(); }));
  runner().OnAutofillManagerStateChanged(
      manager(), AutofillManager::LifecycleState::kActive,
      AutofillManager::LifecycleState::kPendingReset);
  std::move(load_reply).Run(true);
  agent.Flush();
}

// Tests that a reply to `DetectCustomFormControls()` that arrives after the
// frame's manager was reset for a new document isn't stored.
TEST_F(CustomFormControlScriptRunnerTest, IgnoresDetectReplyAfterReset) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  MockCustomFormControlAgent& agent = BindAgent();
  mojom::CustomFormControlAgent::DetectCustomFormControlsCallback detect_reply;
  EXPECT_CALL(agent, DetectCustomFormControls)
      .WillOnce(MoveArg<1>(&detect_reply));
  SeeForms();
  ASSERT_TRUE(base::test::RunUntil([&] { return !detect_reply.is_null(); }));
  runner().OnAutofillManagerStateChanged(
      manager(), AutofillManager::LifecycleState::kActive,
      AutofillManager::LifecycleState::kPendingReset);
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  controls.push_back(MakeControl(1, "ADDRESS_HOME_STATE"));
  std::move(detect_reply).Run(std::move(controls));
  agent.Flush();
  EXPECT_THAT(test_api(runner()).detected_controls(), IsEmpty());
}

// Tests that the detected controls are cleared when the main frame's manager is
// reset for a new document.
TEST_F(CustomFormControlScriptRunnerTest, ClearsDetectedControlsOnReset) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  controls.push_back(MakeControl(1, "ADDRESS_HOME_STATE"));
  ASSERT_TRUE(DetectControls(std::move(controls)));
  runner().OnAutofillManagerStateChanged(
      manager(), AutofillManager::LifecycleState::kActive,
      AutofillManager::LifecycleState::kPendingReset);
  EXPECT_THAT(test_api(runner()).detected_controls(), IsEmpty());
}

// Tests that the detected controls are cleared when the main frame's document
// becomes inactive (e.g. it is put into the BFCache).
TEST_F(CustomFormControlScriptRunnerTest, ClearsDetectedControlsWhenInactive) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  controls.push_back(MakeControl(1, "ADDRESS_HOME_STATE"));
  ASSERT_TRUE(DetectControls(std::move(controls)));
  runner().OnAutofillManagerStateChanged(
      manager(), AutofillManager::LifecycleState::kActive,
      AutofillManager::LifecycleState::kInactive);
  EXPECT_THAT(test_api(runner()).detected_controls(), IsEmpty());
}

// Tests that resetting a subframe's manager doesn't clear the main frame's
// detected controls.
TEST_F(CustomFormControlScriptRunnerTest,
       KeepsDetectedControlsOnSubframeReset) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://example.com/checkout"));
  std::vector<mojom::DetectedCustomFormControlPtr> controls;
  controls.push_back(MakeControl(1, "ADDRESS_HOME_STATE"));
  ASSERT_TRUE(DetectControls(std::move(controls)));
  content::RenderFrameHost* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  child = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/iframe"), child);
  runner().OnAutofillManagerStateChanged(
      manager(child), AutofillManager::LifecycleState::kActive,
      AutofillManager::LifecycleState::kPendingReset);
  EXPECT_THAT(test_api(runner()).detected_controls(), SizeIs(1));
}

// Tests that no script is found for URLs without one.
TEST_F(CustomFormControlScriptRunnerTest, IgnoresUrlsWithoutScript) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://other.com/checkout"));
  SeeForms();
  EXPECT_THAT(found_scripts(), IsEmpty());
}

// Tests that no script is searched for documents of other frames than the main
// frame.
TEST_F(CustomFormControlScriptRunnerTest, IgnoresSubframes) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://other.com/checkout"));
  content::RenderFrameHost* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  child = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/checkout"), child);
  runner().OnAfterFormsSeen(manager(child), {test::MakeFormGlobalId()}, {});
  EXPECT_THAT(found_scripts(), IsEmpty());
}

// Tests that forms seen before the index is initialized don't count as a
// lookup, so that forms seen later in the same document are looked up.
TEST_F(CustomFormControlScriptRunnerTest, WaitsForIndex) {
  NavigateAndCommit(GURL("https://example.com/checkout"));
  SeeForms();
  EXPECT_THAT(found_scripts(), IsEmpty());
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  SeeForms();
  EXPECT_THAT(found_scripts(), ElementsAre("example script"));
}

}  // namespace
}  // namespace autofill
