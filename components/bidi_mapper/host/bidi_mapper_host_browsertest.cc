// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bidi_mapper/host/bidi_mapper_host.h"

#include <stdint.h>

#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/bidi_mapper/host/bidi_mapper_ui.h"
#include "components/bidi_mapper/host/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "url/gurl.h"

namespace bidi_mapper {

namespace {

using base::test::ParseJsonDict;
using testing::IsEmpty;
using testing::SizeIs;
using FailureReason = BidiMapperHost::FailureReason;

constexpr int32_t kPrivilegedFeatureId = 42;

void AddComponentsTestResources() {
  base::FilePath pak_dir;
#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &pak_dir));
#else
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_dir));
#endif
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_dir.Append(FILE_PATH_LITERAL("components_tests_resources.pak")),
      ui::kScaleFactorNone);
}

// A client of a BidiMapperHost that sends BiDi commands and collects what the
// host sends back.
class TestClient : public BidiMapperHost::Client {
 public:
  explicit TestClient(content::BrowserContext* browser_context,
                      int32_t privileged_feature_id = kPrivilegedFeatureId)
      : host_(BidiMapperHost::Create(browser_context,
                                     privileged_feature_id,
                                     this)) {}

  BidiMapperHost* host() { return host_.get(); }
  void DestroyHost() { host_.reset(); }

  bool is_closed() const { return is_closed_; }
  const std::vector<base::DictValue>& events() const { return events_; }

  bool WaitUntilStarted() { return !start_outcome_.Get().has_value(); }

  // Sends a command and returns the response to it.
  base::DictValue SendCommand(std::string_view method,
                              std::string_view params = "{}") {
    const int id = ++last_command_id_;
    base::DictValue command;
    command.Set("id", id);
    command.Set("method", method);
    command.Set("params", ParseJsonDict(params));
    host_->SendBidiMessage(*base::WriteJson(command));
    if (!base::test::RunUntil(
            [&] { return responses_.contains(id) || is_closed_; }) ||
        !responses_.contains(id)) {
      ADD_FAILURE() << "No response to " << method;
      return base::DictValue();
    }
    return std::move(responses_.extract(id).mapped());
  }

  // Creates a BiDi session. Returns true on success.
  bool CreateSession() {
    base::DictValue response =
        SendCommand("session.new", R"({"capabilities":{}})");
    const std::string* type = response.FindString("type");
    return type && *type == "success";
  }

  bool WaitUntilClosed() {
    return base::test::RunUntil([&] { return is_closed_; });
  }

 private:
  // BidiMapperHost::Client:
  void OnStarted() override { start_outcome_.SetValue(std::nullopt); }

  void OnFailed(FailureReason reason) override {
    start_outcome_.SetValue(reason);
  }

  void OnMessage(const std::string& message) override {
    base::DictValue dict = ParseJsonDict(message);
    if (std::optional<int> id = dict.FindInt("id")) {
      responses_[*id] = std::move(dict);
    } else {
      events_.push_back(std::move(dict));
    }
  }

  void OnClosed() override { is_closed_ = true; }

  std::unique_ptr<BidiMapperHost> host_;
  base::test::TestFuture<std::optional<FailureReason>> start_outcome_;
  int last_command_id_ = 0;
  std::map<int, base::DictValue> responses_;
  std::vector<base::DictValue> events_;
  bool is_closed_ = false;
};

// Returns the error code of the response to a BiDi command, or an empty string
// if the command succeeded.
std::string GetError(const base::DictValue& response) {
  const std::string* error = response.FindString("error");
  return error ? *error : std::string();
}

// Returns the top-level browsing contexts that the session of `client` sees.
base::ListValue GetContexts(TestClient& client) {
  base::DictValue response = client.SendCommand("browsingContext.getTree");
  base::ListValue* contexts = response.FindListByDottedPath("result.contexts");
  if (!contexts) {
    ADD_FAILURE() << "Unexpected response: " << response;
    return base::ListValue();
  }
  return std::move(*contexts);
}

// Navigates the browsing context with the given id to `url`, and waits until
// the page is loaded. Returns the error code of the response, or an empty
// string on success.
std::string Navigate(TestClient& client,
                     const std::string& context,
                     const GURL& url) {
  base::DictValue params;
  params.Set("context", context);
  params.Set("url", url.spec());
  params.Set("wait", "complete");
  return GetError(
      client.SendCommand("browsingContext.navigate", *base::WriteJson(params)));
}

// Returns the number of elements that match the CSS selector `selector` in
// the browsing context with the given id, or -1 on failure. The mapper has to
// reach into the page to answer.
int CountElements(TestClient& client,
                  const std::string& context,
                  std::string_view selector) {
  base::DictValue params;
  params.Set("context", context);
  params.SetByDottedPath("locator.type", "css");
  params.SetByDottedPath("locator.value", selector);
  base::DictValue response = client.SendCommand("browsingContext.locateNodes",
                                                *base::WriteJson(params));
  const base::ListValue* nodes = response.FindListByDottedPath("result.nodes");
  if (!nodes) {
    ADD_FAILURE() << "Unexpected response: " << response;
    return -1;
  }
  return static_cast<int>(nodes->size());
}

// Records what the WebContents of a mapper is like at the moment the last
// DevTools client detaches from it. The DevTools protocol connection that a
// host grants to the mapper document is one of these clients.
class MapperDetachObserver : public content::DevToolsAgentHostObserver {
 public:
  explicit MapperDetachObserver(content::WebContents* mapper)
      : agent_host_(content::DevToolsAgentHost::GetOrCreateFor(mapper)) {
    content::DevToolsAgentHost::AddObserver(this);
  }
  MapperDetachObserver(const MapperDetachObserver&) = delete;
  MapperDetachObserver& operator=(const MapperDetachObserver&) = delete;
  ~MapperDetachObserver() override {
    content::DevToolsAgentHost::RemoveObserver(this);
  }

  bool detached() const { return detached_; }
  // True if the clients were detached only because the WebContents was going
  // away.
  bool was_being_destroyed() const { return was_being_destroyed_; }
  // The address of the document that the WebContents had.
  const GURL& committed_url() const { return committed_url_; }

 private:
  // content::DevToolsAgentHostObserver:
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override {
    if (agent_host != agent_host_.get() || detached_) {
      return;
    }
    detached_ = true;
    content::WebContents* web_contents = agent_host->GetWebContents();
    was_being_destroyed_ = !web_contents || web_contents->IsBeingDestroyed();
    if (web_contents) {
      committed_url_ = web_contents->GetLastCommittedURL();
    }
  }

  const scoped_refptr<content::DevToolsAgentHost> agent_host_;
  bool detached_ = false;
  bool was_being_destroyed_ = false;
  GURL committed_url_;
};

class BidiMapperHostBrowserTest : public content::ContentBrowserTest {
 public:
  BidiMapperHostBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kChromeDriverExtensionApi);
  }

 protected:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    AddComponentsTestResources();

    // The mapper sets the download behavior over the DevTools protocol when it
    // starts, which puts a delegate of DevTools in front of the delegate of
    // the download manager. The shell destroys its delegate before the one of
    // DevTools, which points to it. Use a delegate that outlives both instead,
    // the way DevToolsDownloadContentTest does.
    content::DownloadManager* download_manager =
        browser_context()->GetDownloadManager();
    download_manager->GetDelegate()->Shutdown();
    download_manager_delegate_ =
        std::make_unique<content::ShellDownloadManagerDelegate>();
    download_manager->SetDelegate(download_manager_delegate_.get());
    download_manager_delegate_->SetDownloadManager(download_manager);
  }

  content::BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  // Returns the WebContents that run the mappers of the hosts.
  std::vector<content::WebContents*> GetMapperWebContents() {
    std::vector<content::WebContents*> mappers;
    for (content::WebContents* web_contents : content::GetAllWebContents()) {
      if (web_contents->IsPrivileged()) {
        mappers.push_back(web_contents);
      }
    }
    return mappers;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::ScopedWebUIConfigRegistration webui_config_registration_{
      std::make_unique<BidiMapperUIConfig>()};
  std::unique_ptr<content::ShellDownloadManagerDelegate>
      download_manager_delegate_;
};

IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, StartAndStop) {
  ASSERT_TRUE(BidiMapperHost::IsEnabled());
  TestClient client(browser_context());
  ASSERT_TRUE(client.host());
  ASSERT_TRUE(client.WaitUntilStarted());
  std::vector<content::WebContents*> mappers = GetMapperWebContents();
  ASSERT_THAT(mappers, SizeIs(1));
  EXPECT_EQ(mappers[0]->GetLastCommittedURL(), GURL(kBidiMapperUIURL));
  EXPECT_EQ(mappers[0]->GetBrowserContext(), browser_context());
  EXPECT_EQ(mappers[0]->GetVisibility(), content::Visibility::HIDDEN);
  // The message filter took the bindings of the host away from the document,
  // so the mapper has no way to the client around the filter.
  EXPECT_EQ("undefined",
            content::EvalJs(mappers[0], "typeof window.bidiHostSendMessage"));
  EXPECT_EQ("undefined",
            content::EvalJs(mappers[0], "typeof window.bidiHostEndSession"));

  client.DestroyHost();
  EXPECT_THAT(GetMapperWebContents(), IsEmpty());
  // The client that destroys the host is not told that it closed.
  EXPECT_FALSE(client.is_closed());
}

// The mapper document has a DevTools protocol connection to the whole browser,
// so the host runs it in a privileged WebContents, see
// content::WebContents::PrivilegedParams.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, MapperIsPrivileged) {
  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  std::vector<content::WebContents*> mappers = GetMapperWebContents();
  ASSERT_THAT(mappers, SizeIs(1));
  EXPECT_TRUE(mappers[0]->IsPrivileged());
  content::RenderProcessHost* mapper_process =
      mappers[0]->GetPrimaryMainFrame()->GetProcess();
  EXPECT_TRUE(mapper_process->IsPrivileged());

  // The mapper document may not use shared workers.
  EXPECT_EQ(false, content::EvalJs(mappers[0], "'SharedWorker' in window"));

  // A host created with a different privileged feature id runs its mapper in a
  // separate privileged process.
  {
    TestClient other_client(browser_context(), kPrivilegedFeatureId + 1);
    ASSERT_TRUE(other_client.WaitUntilStarted());
    std::vector<content::WebContents*> both_mappers = GetMapperWebContents();
    ASSERT_THAT(both_mappers, SizeIs(2));
    content::WebContents* other_mapper =
        both_mappers[0] == mappers[0] ? both_mappers[1] : both_mappers[0];
    content::RenderProcessHost* other_process =
        other_mapper->GetPrimaryMainFrame()->GetProcess();
    EXPECT_TRUE(other_process->IsPrivileged());
    EXPECT_NE(other_process, mapper_process);
  }

  // An ordinary WebContents that shows the same document does so in another
  // process, which is not privileged.
  ASSERT_TRUE(content::NavigateToURL(shell(), GURL(kBidiMapperUIURL)));
  content::RenderProcessHost* shell_process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(shell_process, mapper_process);
  EXPECT_FALSE(shell_process->IsPrivileged());
  EXPECT_EQ(true, content::EvalJs(shell(), "'SharedWorker' in window"));

  // The mapper works where it is.
  EXPECT_TRUE(client.CreateSession());
}

IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, Session) {
  const GURL url("data:text/html,<title>page</title>");
  ASSERT_TRUE(content::NavigateToURL(shell(), url));

  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());

  base::DictValue status = client.SendCommand("session.status");
  EXPECT_EQ(status.FindBoolByDottedPath("result.ready"), true);
  // There is no session yet.
  EXPECT_EQ(GetError(client.SendCommand("browsingContext.getTree")),
            "invalid session id");

  base::DictValue session =
      client.SendCommand("session.new", R"({"capabilities":{}})");
  EXPECT_EQ(GetError(session), "");
  const std::string* session_id =
      session.FindStringByDottedPath("result.sessionId");
  ASSERT_TRUE(session_id) << session;
  EXPECT_TRUE(base::Uuid::ParseLowercase(*session_id).is_valid())
      << *session_id;

  status = client.SendCommand("session.status");
  EXPECT_EQ(status.FindBoolByDottedPath("result.ready"), false);
  EXPECT_EQ(
      GetError(client.SendCommand("session.new", R"({"capabilities":{}})")),
      "session not created");

  // The session sees the page of the shell, and not the mapper.
  base::ListValue contexts = GetContexts(client);
  ASSERT_EQ(contexts.size(), 1u);
  const base::DictValue& context = contexts[0].GetDict();
  ASSERT_TRUE(context.FindString("url"));
  EXPECT_EQ(*context.FindString("url"), url.spec());
  ASSERT_TRUE(context.FindString("context"));
  // The session reaches into the page.
  EXPECT_EQ(CountElements(client, *context.FindString("context"), "title"), 1);

  // Ending the session closes the host.
  base::DictValue end = client.SendCommand("session.end");
  EXPECT_EQ(GetError(end), "");
  EXPECT_TRUE(client.WaitUntilClosed());
  EXPECT_THAT(GetMapperWebContents(), IsEmpty());
}

// A session keeps driving a page that it navigates to another site, which
// replaces the frame that the mapper is attached to.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, Navigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(content::NavigateToURL(
      shell(), GURL("data:text/html,<title>page</title>")));

  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(client.CreateSession());
  base::ListValue contexts = GetContexts(client);
  ASSERT_EQ(contexts.size(), 1u);
  const std::string* context_id = contexts[0].GetDict().FindString("context");
  ASSERT_TRUE(context_id);
  const std::string context = *context_id;

  const GURL url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_EQ(Navigate(client, context, url), "");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url);
  // The session reaches into the new page.
  EXPECT_EQ(CountElements(client, context, "body"), 1);

  // The session still sees the page of the shell only, at its new address.
  contexts = GetContexts(client);
  ASSERT_EQ(contexts.size(), 1u);
  const std::string* context_url = contexts[0].GetDict().FindString("url");
  ASSERT_TRUE(context_url);
  EXPECT_EQ(*context_url, url.spec());
}

// Hosts serve sessions independently, and do not show up in the sessions of
// one another.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, TwoHosts) {
  ASSERT_TRUE(
      content::NavigateToURL(shell(), GURL("data:text/html,<title>a</title>")));

  TestClient first(browser_context());
  ASSERT_TRUE(first.WaitUntilStarted());
  ASSERT_TRUE(first.CreateSession());
  EXPECT_EQ(GetError(first.SendCommand(
                "session.subscribe",
                R"({"events":["browsingContext.contextCreated"]})")),
            "");

  // The first session attaches to every new page and pauses it. The mapper of
  // another host has to start regardless.
  TestClient second(browser_context());
  ASSERT_TRUE(second.WaitUntilStarted());
  ASSERT_TRUE(second.CreateSession());
  EXPECT_THAT(GetMapperWebContents(), SizeIs(2));

  // Both sessions see the page of the shell only.
  base::ListValue first_contexts = GetContexts(first);
  base::ListValue second_contexts = GetContexts(second);
  ASSERT_EQ(first_contexts.size(), 1u);
  ASSERT_EQ(second_contexts.size(), 1u);
  const std::string* context =
      first_contexts[0].GetDict().FindString("context");
  ASSERT_TRUE(context);
  EXPECT_EQ(first_contexts, second_contexts);
  // The first session was told about the page of the shell when it
  // subscribed, and was not told about the mapper of the second host.
  for (const base::DictValue& event : first.events()) {
    const std::string* event_context =
        event.FindStringByDottedPath("params.context");
    ASSERT_TRUE(event_context) << event;
    EXPECT_EQ(*event_context, *context);
  }

  // Both sessions drive the page.
  const GURL first_url("data:text/html,<title>b</title>");
  EXPECT_EQ(Navigate(first, *context, first_url), "");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), first_url);
  const GURL second_url("data:text/html,<title>c</title>");
  EXPECT_EQ(Navigate(second, *context, second_url), "");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), second_url);

  // Ending a session does not affect the other one.
  EXPECT_EQ(GetError(first.SendCommand("session.end")), "");
  EXPECT_TRUE(first.WaitUntilClosed());
  EXPECT_THAT(GetMapperWebContents(), SizeIs(1));
  EXPECT_FALSE(second.is_closed());
  EXPECT_EQ(CountElements(second, *context, "title"), 1);
}

IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, RefusesCdp) {
  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(client.CreateSession());

  EXPECT_EQ(GetError(client.SendCommand(
                "goog:cdp.sendCommand",
                R"({"method":"Browser.getVersion","params":{}})")),
            "unknown command");
  EXPECT_EQ(GetError(client.SendCommand("goog:cdp.getSession",
                                        R"({"context":"unknown"})")),
            "unknown command");
  EXPECT_EQ(GetError(client.SendCommand("session.subscribe",
                                        R"({"events":["goog:cdp"]})")),
            "invalid argument");
  EXPECT_EQ(GetError(client.SendCommand(
                "session.subscribe",
                R"({"events":["goog:cdp.Target.targetCreated"]})")),
            "invalid argument");

  // The message filter tells the module by the prefix of its name. The mapper
  // knows the module under no other name.
  EXPECT_EQ(
      GetError(client.SendCommand(
          "cdp.sendCommand", R"({"method":"Browser.getVersion","params":{}})")),
      "unknown command");
  EXPECT_EQ(
      GetError(client.SendCommand(
          "session.subscribe", R"({"events":["cdp.Target.targetCreated"]})")),
      "invalid argument");

  // The session works as before, and gets no events of the DevTools protocol.
  EXPECT_EQ(GetContexts(client).size(), 1u);
  EXPECT_THAT(client.events(), IsEmpty());
}

// The message filter refuses the commands that a client may not use. For now
// these are `script.evaluate` and `script.callFunction`.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, RefusesUnsupportedCommands) {
  ASSERT_TRUE(content::NavigateToURL(
      shell(), GURL("data:text/html,<title>page</title>")));

  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(client.CreateSession());
  base::ListValue contexts = GetContexts(client);
  ASSERT_EQ(contexts.size(), 1u);
  const std::string* context = contexts[0].GetDict().FindString("context");
  ASSERT_TRUE(context);

  base::DictValue evaluate_params;
  evaluate_params.Set("expression", "document.title = 'changed'");
  evaluate_params.Set("awaitPromise", false);
  evaluate_params.SetByDottedPath("target.context", *context);
  EXPECT_EQ(GetError(client.SendCommand("script.evaluate",
                                        *base::WriteJson(evaluate_params))),
            "unsupported operation");

  base::DictValue call_params;
  call_params.Set("functionDeclaration",
                  "() => { document.title = 'changed'; }");
  call_params.Set("awaitPromise", false);
  call_params.SetByDottedPath("target.context", *context);
  EXPECT_EQ(GetError(client.SendCommand("script.callFunction",
                                        *base::WriteJson(call_params))),
            "unsupported operation");

  // The page is as it was, and the session works as before.
  EXPECT_EQ("page", content::EvalJs(shell(), "document.title"));
  EXPECT_EQ(CountElements(client, *context, "title"), 1);
}

// The host evaluates a call in the mapper document to pass a message on. The
// message is a string there, whatever it holds.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, MessagesAreNotEvaluated) {
  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  std::vector<content::WebContents*> mappers = GetMapperWebContents();
  ASSERT_THAT(mappers, SizeIs(1));

  // None of these is JSON. Each would end or break the string that it is
  // passed in if the host did not escape it.
  const std::string messages[] = {
      R"("); window.injected = true; (")",
      R"(\"); window.injected = true; (\")",
      "\"\n); window.injected = true; (\n\"",
      // The same with U+2028, another line terminator.
      "\"\xE2\x80\xA8); window.injected = true; (\xE2\x80\xA8\"",
      std::string(1, '\0'),
      // Not UTF-8.
      "\xFF",
  };
  for (const std::string& message : messages) {
    client.host()->SendBidiMessage(message);
  }
  // The errors have no id, so the client keeps them with the events.
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return client.events().size() >= std::size(messages); }));
  EXPECT_THAT(client.events(), SizeIs(std::size(messages)));
  for (const base::DictValue& error : client.events()) {
    EXPECT_EQ(GetError(error), "invalid argument") << error;
  }
  EXPECT_EQ(false, content::EvalJs(mappers[0], "'injected' in window"));

  // The host works as before.
  EXPECT_TRUE(client.CreateSession());
}

IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, ClosedWhenMapperCrashes) {
  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(client.CreateSession());

  std::vector<content::WebContents*> mappers = GetMapperWebContents();
  ASSERT_THAT(mappers, SizeIs(1));
  content::RenderProcessHost* process =
      mappers[0]->GetPrimaryMainFrame()->GetProcess();
  content::ScopedAllowRendererCrashes allow_renderer_crashes(process);
  content::RenderProcessHostWatcher watcher(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(content::RESULT_CODE_KILLED);
  watcher.Wait();

  EXPECT_TRUE(client.WaitUntilClosed());
  EXPECT_THAT(GetMapperWebContents(), IsEmpty());
  client.DestroyHost();
}

// The DevTools protocol connection of a mapper is for the mapper document
// only. When something navigates the WebContents of a mapper, the host takes
// the connection away before another document commits there, and closes.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, ClosedWhenMapperIsNavigated) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(client.CreateSession());
  std::vector<content::WebContents*> mappers = GetMapperWebContents();
  ASSERT_THAT(mappers, SizeIs(1));
  MapperDetachObserver observer(mappers[0]);

  mappers[0]->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(client.WaitUntilClosed());
  EXPECT_THAT(GetMapperWebContents(), IsEmpty());

  // The clients of the mapper were detached while its document was still the
  // one in the WebContents, and not by the destruction of the WebContents.
  ASSERT_TRUE(observer.detached());
  EXPECT_FALSE(observer.was_being_destroyed());
  EXPECT_EQ(observer.committed_url(), GURL(kBidiMapperUIURL));
}

IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, MapperDocumentExists) {
  EXPECT_TRUE(content::WebUIConfigMap::GetInstance().GetConfig(
      browser_context(), GURL(kBidiMapperUIURL)));
}

// DevTools detaches every client when it is disabled at runtime: the host, and
// the connection of the mapper document. The host closes.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, ClosedWhenDetached) {
  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(client.CreateSession());
  std::vector<content::WebContents*> mappers = GetMapperWebContents();
  ASSERT_THAT(mappers, SizeIs(1));
  MapperDetachObserver observer(mappers[0]);

  content::DevToolsAgentHost::DetachAllClients();
  EXPECT_TRUE(client.WaitUntilClosed());
  EXPECT_THAT(GetMapperWebContents(), IsEmpty());

  // DevTools took the connection of the mapper document away when it detached
  // the host, and did not leave that to the destruction of the WebContents.
  ASSERT_TRUE(observer.detached());
  EXPECT_FALSE(observer.was_being_destroyed());
}

// A host closes before its client is told, so the client may send a message
// to a host that is closed already. The message is dropped.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest,
                       MessageToClosedHostIsDropped) {
  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(client.CreateSession());

  // The host closes here, and tells the client in a later task.
  content::DevToolsAgentHost::DetachAllClients();
  ASSERT_FALSE(client.is_closed());
  client.host()->SendBidiMessage(
      R"({"id":100,"method":"session.status","params":{}})");

  EXPECT_TRUE(client.WaitUntilClosed());
}

// Runs all tasks already queued on the current SequencedTaskRunner.
void FlushCurrentTaskRunner() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// When a host closes and its owner destroys both the host and its
// BrowserContext in the same task, the mapper WebContents must be destroyed
// with the host rather than outliving the BrowserContext in a posted task.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest,
                       ClosedHostDestroyedBeforeBrowserContext) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  content::ShellDownloadManagerDelegate download_delegate;
  auto context =
      std::make_unique<content::ShellBrowserContext>(/*off_the_record=*/true);
  content::DownloadManager* download_manager = context->GetDownloadManager();
  download_manager->GetDelegate()->Shutdown();
  download_manager->SetDelegate(&download_delegate);
  download_delegate.SetDownloadManager(download_manager);

  TestClient client(context.get());
  ASSERT_TRUE(client.WaitUntilStarted());
  ASSERT_TRUE(client.CreateSession());
  ASSERT_THAT(GetMapperWebContents(), SizeIs(1));

  // Trigger Close(), which defers WebContents destruction, then immediately
  // destroy the host and BrowserContext without spinning the message loop.
  content::DevToolsAgentHost::DetachAllClients();
  client.DestroyHost();
  EXPECT_THAT(GetMapperWebContents(), IsEmpty());
  context.reset();

  FlushCurrentTaskRunner();
}

// BrowserContext::NotifyWillBeDestroyed() cancels in-flight navigations before
// KeyedServices are torn down, causing a starting host to call Close() from
// DidFinishNavigation(). Destroying the host before ~BrowserContext() completes
// must synchronously destroy the mapper WebContents.
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest,
                       StartingHostDestroyedDuringBrowserContextShutdown) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto context =
      std::make_unique<content::ShellBrowserContext>(/*off_the_record=*/true);

  TestClient client(context.get());
  ASSERT_TRUE(client.host());
  ASSERT_THAT(GetMapperWebContents(), SizeIs(1));

  context->NotifyWillBeDestroyed();
  client.DestroyHost();
  EXPECT_THAT(GetMapperWebContents(), IsEmpty());
  context.reset();

  FlushCurrentTaskRunner();
}

// The client may destroy the host from within any callback, and never hears
// from the host again afterwards (even if another callback was already queued).
IN_PROC_BROWSER_TEST_F(BidiMapperHostBrowserTest, DestroyedInClientCallbacks) {
  enum class DestroyPoint { kOnStarted, kOnFailed, kOnMessage, kOnClosed };

  class SelfDestroyingClient : public BidiMapperHost::Client {
   public:
    SelfDestroyingClient(content::BrowserContext* browser_context,
                         DestroyPoint destroy_point)
        : destroy_point_(destroy_point),
          host_(BidiMapperHost::Create(browser_context,
                                       kPrivilegedFeatureId,
                                       this)) {}

    bool WaitUntilDestroyed() { return destroyed_.Wait(); }

   private:
    void Destroy() {
      ASSERT_TRUE(host_);
      host_.reset();
      destroyed_.SetValue();
    }

    void OnStarted() override {
      ASSERT_TRUE(host_);
      if (destroy_point_ == DestroyPoint::kOnStarted) {
        Destroy();
        return;
      }
      host_->SendBidiMessage(
          R"({"id":1,"method":"session.new","params":{"capabilities":{}}})");
    }

    void OnFailed(FailureReason reason) override {
      ASSERT_TRUE(host_);
      EXPECT_EQ(destroy_point_, DestroyPoint::kOnFailed);
      EXPECT_EQ(reason, FailureReason::kFailed);
      Destroy();
    }

    void OnMessage(const std::string& message) override {
      ASSERT_TRUE(host_);
      base::DictValue dict = ParseJsonDict(message);
      std::optional<int> id = dict.FindInt("id");
      if (id == 1) {
        host_->SendBidiMessage(
            R"({"id":2,"method":"session.end","params":{}})");
      } else if (id == 2 && destroy_point_ == DestroyPoint::kOnMessage) {
        // `session.end` has already queued OnClosed(); destroying the host here
        // must cancel it.
        Destroy();
      }
    }

    void OnClosed() override {
      ASSERT_TRUE(host_);
      EXPECT_EQ(destroy_point_, DestroyPoint::kOnClosed);
      Destroy();
    }

    const DestroyPoint destroy_point_;
    std::unique_ptr<BidiMapperHost> host_;
    base::test::TestFuture<void> destroyed_;
  };

  {
    SelfDestroyingClient client(browser_context(), DestroyPoint::kOnStarted);
    EXPECT_TRUE(client.WaitUntilDestroyed());
    EXPECT_THAT(GetMapperWebContents(), IsEmpty());
    FlushCurrentTaskRunner();
  }
  {
    SelfDestroyingClient client(browser_context(), DestroyPoint::kOnFailed);
    std::vector<content::WebContents*> mappers = GetMapperWebContents();
    ASSERT_THAT(mappers, SizeIs(1));
    mappers[0]->Stop();
    EXPECT_TRUE(client.WaitUntilDestroyed());
    EXPECT_THAT(GetMapperWebContents(), IsEmpty());
    FlushCurrentTaskRunner();
  }
  {
    SelfDestroyingClient client(browser_context(), DestroyPoint::kOnMessage);
    EXPECT_TRUE(client.WaitUntilDestroyed());
    EXPECT_THAT(GetMapperWebContents(), IsEmpty());
    FlushCurrentTaskRunner();
  }
  {
    SelfDestroyingClient client(browser_context(), DestroyPoint::kOnClosed);
    EXPECT_TRUE(client.WaitUntilDestroyed());
    EXPECT_THAT(GetMapperWebContents(), IsEmpty());
    FlushCurrentTaskRunner();
  }
}

// BrowserTestBase passes --dom-automation, which eagerly initializes the main
// world V8 context on commit. In production, the empty mapper document has no
// V8 context until the host evaluates scripts in it.
class BidiMapperHostWithoutDomAutomationBrowserTest
    : public BidiMapperHostBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    BidiMapperHostBrowserTest::SetUpInProcessBrowserTestFixture();
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDomAutomationController);
  }
};

IN_PROC_BROWSER_TEST_F(BidiMapperHostWithoutDomAutomationBrowserTest,
                       StartAndCreateSession) {
  TestClient client(browser_context());
  ASSERT_TRUE(client.WaitUntilStarted());
  EXPECT_TRUE(client.CreateSession());
}

// Without the feature, there are no hosts and the mapper document does not
// exist.
class BidiMapperHostDisabledBrowserTest : public content::ContentBrowserTest {
 private:
  content::ScopedWebUIConfigRegistration webui_config_registration_{
      std::make_unique<BidiMapperUIConfig>()};
};

IN_PROC_BROWSER_TEST_F(BidiMapperHostDisabledBrowserTest, NothingExists) {
  content::BrowserContext* browser_context =
      shell()->web_contents()->GetBrowserContext();
  EXPECT_FALSE(BidiMapperHost::IsEnabled());
  TestClient client(browser_context);
  EXPECT_FALSE(client.host());
  EXPECT_FALSE(content::WebUIConfigMap::GetInstance().GetConfig(
      browser_context, GURL(kBidiMapperUIURL)));
}

}  // namespace

}  // namespace bidi_mapper
