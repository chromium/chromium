// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/mock_ui_controller.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/service.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::ReturnRef;
using ::testing::Sequence;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace {

class FakeClient : public Client {
 public:
  explicit FakeClient(std::unique_ptr<UiController> ui_controller)
      : ui_controller_(std::move(ui_controller)) {}

  // Implements Client
  std::string GetApiKey() override { return ""; }
  AccessTokenFetcher* GetAccessTokenFetcher() override { return nullptr; }
  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return nullptr;
  }
  std::string GetServerUrl() override { return ""; }
  UiController* GetUiController() override { return ui_controller_.get(); }

 private:
  std::unique_ptr<UiController> ui_controller_;
};

}  // namespace

class ControllerTest : public content::RenderViewHostTestHarness {
 public:
  ControllerTest() {}
  ~ControllerTest() override {}

  static Controller* CreateController(
      content::WebContents* web_contents,
      std::unique_ptr<Client> client,
      std::unique_ptr<WebController> web_controller,
      std::unique_ptr<Service> service,
      std::unique_ptr<std::map<std::string, std::string>> parameters,
      const GURL& initialUrl) {
    return new Controller(web_contents, std::move(client),
                          std::move(web_controller), std::move(service),
                          std::move(parameters), initialUrl);
  }

  static void DestroyController(Controller* controller) {
    controller->OnDestroy();
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    auto ui_controller = std::make_unique<NiceMock<MockUiController>>();
    mock_ui_controller_ = ui_controller.get();
    auto web_controller = std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = web_controller.get();
    auto service = std::make_unique<NiceMock<MockService>>();
    mock_service_ = service.get();
    auto parameters = std::make_unique<std::map<std::string, std::string>>();
    parameters->insert(std::make_pair("a", "b"));
    GURL initialUrl("");

    controller_ = new Controller(
        web_contents(), std::make_unique<FakeClient>(std::move(ui_controller)),
        std::move(web_controller), std::move(service), std::move(parameters),
        initialUrl);

    // Fetching scripts succeeds for all URLs, but return nothing.
    ON_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillByDefault(RunOnceCallback<2>(true, ""));

    // Scripts run, but have no actions.
    ON_CALL(*mock_service_, OnGetActions(_, _, _))
        .WillByDefault(RunOnceCallback<2>(true, ""));

    // Make WebController::GetUrl accessible.
    ON_CALL(*mock_web_controller_, GetUrl()).WillByDefault(ReturnRef(url_));

    tester_ = content::WebContentsTester::For(web_contents());
  }

  void TearDown() override {
    DestroyController(controller_);  // deletes the controller and mocks
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  static SupportedScriptProto* AddRunnableScript(
      SupportsScriptResponseProto* response,
      const std::string& name_and_path) {
    SupportedScriptProto* script = response->add_scripts();
    script->set_path(name_and_path);
    script->mutable_presentation()->set_name(name_and_path);
    return script;
  }

  void SetLastCommittedUrl(const GURL& url) {
    url_ = url;
    tester_->SetLastCommittedURL(url);
  }

  // Updates the current url of the controller and forces a refresh, without
  // bothering with actually rendering any page content.
  void SimulateNavigateToUrl(const GURL& url) {
    SetLastCommittedUrl(url);
    controller_->DidFinishLoad(nullptr, url);
  }

  void SimulateProgressChanged(double progress) {
    controller_->LoadProgressChanged(web_contents(), progress);
  }

  void SimulateUserInteraction(const blink::WebInputEvent::Type type) {
    controller_->DidGetUserInteraction(type);
  }

  // Sets up the next call to the service for scripts to return |response|.
  void SetNextScriptResponse(const SupportsScriptResponseProto& response) {
    std::string response_str;
    response.SerializeToString(&response_str);

    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillOnce(RunOnceCallback<2>(true, response_str));
  }

  UiDelegate* GetUiDelegate() { return controller_; }

  GURL url_;
  MockService* mock_service_;
  MockWebController* mock_web_controller_;
  MockUiController* mock_ui_controller_;
  content::WebContentsTester* tester_;

  // |controller_| deletes itself when OnDestroy is called from Setup. Outside
  // of tests, the controller deletes itself when the web contents it observers
  // is destroyed or when UiDelegate::OnDestroy is called.
  Controller* controller_;
};

TEST_F(ControllerTest, FetchAndRunScripts) {
  // Going to the URL triggers a whole flow:
  // 1. loading scripts
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");
  AddRunnableScript(&script_response, "script2");
  SetNextScriptResponse(script_response);

  // 2. checking the scripts
  // 3. offering the choices: script1 and script2
  // 4. script1 is chosen
  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(2)))
      .WillOnce([this](const std::vector<ScriptHandle>& scripts) {
        std::vector<std::string> paths;
        for (const auto& script : scripts) {
          paths.emplace_back(script.path);
        }
        EXPECT_THAT(paths, UnorderedElementsAre("script1", "script2"));

        Sequence sequence;
        // Selecting a script should clean the bottom bar.
        EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(0)))
            .InSequence(sequence);
        // After the script is done both scripts are again valid and should be
        // shown.
        EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(2)))
            .InSequence(sequence);

        GetUiDelegate()->OnScriptSelected("script1");
      });

  // 5. script1 run successfully (no actions).
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("script1"), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  // 6. As nothing is selected the flow terminates.

  // Start the flow.
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, ShowFirstInitialPrompt) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "script1");

  SupportedScriptProto* script2 =
      AddRunnableScript(&script_response, "script2");
  script2->mutable_presentation()->set_initial_prompt("script2 prompt");
  script2->mutable_presentation()->set_priority(10);

  SupportedScriptProto* script3 =
      AddRunnableScript(&script_response, "script3");
  script3->mutable_presentation()->set_initial_prompt("script3 prompt");
  script3->mutable_presentation()->set_priority(5);

  SupportedScriptProto* script4 =
      AddRunnableScript(&script_response, "script4");
  script4->mutable_presentation()->set_initial_prompt("script4 prompt");
  script4->mutable_presentation()->set_priority(8);

  SetNextScriptResponse(script_response);

  // Script3, with higher priority (lower number), wins.
  EXPECT_CALL(*mock_ui_controller_, ShowStatusMessage("script3 prompt"));
  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(4)));

  // Start the flow.
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, Stop) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();
  std::string actions_response_str;
  actions_response.SerializeToString(&actions_response_str);
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("stop"), _, _))
      .WillOnce(RunOnceCallback<2>(true, actions_response_str));
  EXPECT_CALL(*mock_service_, OnGetNextActions(_, _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  EXPECT_CALL(*mock_ui_controller_, Shutdown());
  GetUiDelegate()->OnScriptSelected("stop");
}

TEST_F(ControllerTest, Reset) {
  {
    InSequence sequence;

    // 1. Fetch scripts for URL, which in contains a single "reset" script.
    SupportsScriptResponseProto script_response;
    AddRunnableScript(&script_response, "reset");
    std::string script_response_str;
    script_response.SerializeToString(&script_response_str);
    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillOnce(RunOnceCallback<2>(true, script_response_str));

    EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(1)));

    // 2. Execute the "reset" script, which contains a reset action.

    // Selecting a script should clean the bottom bar.
    EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(0)));

    ActionsResponseProto actions_response;
    actions_response.add_actions()->mutable_reset();
    std::string actions_response_str;
    actions_response.SerializeToString(&actions_response_str);
    EXPECT_CALL(*mock_service_, OnGetActions(StrEq("reset"), _, _))
        .WillOnce(RunOnceCallback<2>(true, actions_response_str));

    // 3. Report the result of running that action.
    EXPECT_CALL(*mock_service_, OnGetNextActions(_, _, _))
        .WillOnce(RunOnceCallback<2>(true, ""));

    // 4. The reset action forces a reload of the scripts, even though the URL
    // hasn't changed. The "reset" script is reported again to UpdateScripts.
    EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillOnce(RunOnceCallback<2>(true, script_response_str));

    // Reset forces the controller to fetch the scripts twice, even though the
    // URL doesn't change..
    EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(1)));
  }

  // Resetting should clear the client memory
  controller_->GetClientMemory()->set_selected_card("set");

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
  GetUiDelegate()->OnScriptSelected("reset");

  EXPECT_FALSE(controller_->GetClientMemory()->selected_card());
}

TEST_F(ControllerTest, RefreshScriptWhenDomainChanges) {
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://a.example.com/path1")), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://b.example.com/path1")), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  SimulateNavigateToUrl(GURL("http://a.example.com/path1"));
  SimulateNavigateToUrl(GURL("http://a.example.com/path2"));
  SimulateNavigateToUrl(GURL("http://b.example.com/path1"));
  SimulateNavigateToUrl(GURL("http://b.example.com/path2"));
}

TEST_F(ControllerTest, ForwardParameters) {
  // Parameter a=b is set in SetUp.
  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(_, Contains(Pair("a", "b")), _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  SimulateNavigateToUrl(GURL("http://example.com/"));
}

TEST_F(ControllerTest, Autostart) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable");
  AddRunnableScript(&script_response, "autostart")
      ->mutable_presentation()
      ->set_autostart(true);
  AddRunnableScript(&script_response, "alsorunnable");
  SetNextScriptResponse(script_response);

  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("autostart"), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, AutostartIsNotPassedToTheUi) {
  SupportsScriptResponseProto script_response;
  AddRunnableScript(&script_response, "runnable")
      ->mutable_presentation()
      ->set_autostart(true);
  SetNextScriptResponse(script_response);

  EXPECT_CALL(*mock_ui_controller_, UpdateScripts(SizeIs(0)));
  EXPECT_CALL(*mock_service_, OnGetActions(StrEq("runnable"), _, _)).Times(0);

  SimulateUserInteraction(blink::WebInputEvent::kTouchStart);
  SimulateNavigateToUrl(GURL("http://a.example.com/path"));
}

TEST_F(ControllerTest, LoadProgressChanged) {
  SetLastCommittedUrl(GURL("http://a.example.com/path"));

  EXPECT_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _)).Times(0);
  SimulateProgressChanged(0.1);
  SimulateProgressChanged(0.3);
  SimulateProgressChanged(0.5);

  EXPECT_CALL(*mock_service_,
              OnGetScriptsForUrl(Eq(GURL("http://a.example.com/path")), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));
  SimulateProgressChanged(0.4);
}

TEST_F(ControllerTest, InitialUrlLoads) {
  GURL initialUrl("http://a.example.com/path");
  auto service = std::make_unique<NiceMock<MockService>>();

  EXPECT_CALL(*service.get(), OnGetScriptsForUrl(Eq(initialUrl), _, _))
      .WillOnce(RunOnceCallback<2>(true, ""));

  Controller* controller = ControllerTest::CreateController(
      web_contents(),
      std::make_unique<FakeClient>(
          std::make_unique<NiceMock<MockUiController>>()),
      std::make_unique<NiceMock<MockWebController>>(), std::move(service),
      std::make_unique<std::map<std::string, std::string>>(), initialUrl);
  ControllerTest::DestroyController(controller);
}

}  // namespace autofill_assistant
