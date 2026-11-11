// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_runner.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "components/autofill/content/browser/integrators/custom_form_controls/mock_custom_form_control_agent.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/custom_form_controls/custom_form_control_script_index.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;

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
  }

  void TearDown() override {
    runner_.reset();
    content::RenderViewHostTestHarness::TearDown();
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

  CustomFormControlScriptRunner& runner() { return *runner_; }

  TestContentAutofillClient& client() {
    return *autofill_client_injector_[web_contents()];
  }

  TestBrowserAutofillManager& manager() {
    return *autofill_manager_injector_[main_rfh()];
  }

  // Binds the main frame's requests for the agent to `agent_` and returns it.
  MockCustomFormControlAgent& BindAgent() {
    main_rfh()->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::CustomFormControlAgent::Name_,
        base::BindRepeating(&MockCustomFormControlAgent::BindPendingReceiver,
                            base::Unretained(&agent_)));
    return agent_;
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

// Tests that the script for the frame's URL is found when forms are seen.
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

// Tests that a frame is looked up again after its manager is reset for a new
// document in the same frame.
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
                                      "example script", testing::_))
      .WillOnce(
          DoAll(RunClosure(run_loop.QuitClosure()), RunOnceCallback<2>(true)));
  SeeForms();
  run_loop.Run();
}

// Tests that no script is found for URLs without one.
TEST_F(CustomFormControlScriptRunnerTest, IgnoresUrlsWithoutScript) {
  ASSERT_TRUE(InitializeIndex("example.com", "example script"));
  NavigateAndCommit(GURL("https://other.com/checkout"));
  SeeForms();
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
