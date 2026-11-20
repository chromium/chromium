// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/values_test_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace bidi_mapper {

namespace {

using base::test::DictionaryHasValue;
using base::test::IsJson;
using base::test::ParseJsonDict;
using testing::AllOf;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;
using testing::SizeIs;

// Stands for the call that tells the host that the session ended, among the
// messages that the filter sends to the host.
constexpr char kSessionEnded[] = "session ended";

// Stands in for the host: defines the bindings that the filter takes, and
// records what the filter does. $1 is `kSessionEnded`.
constexpr char kFakeHostScript[] = R"(
    window.fake = {
      // What the filter sent to the host and to the mapper, in order.
      forHost: [],
      forMapper: [],
      // Returns the above, and starts over.
      take() {
        const {forHost, forMapper} = this;
        this.forHost = [];
        this.forMapper = [];
        return {forHost, forMapper};
      },
    };
    // Like a binding of the DevTools protocol, takes exactly one string.
    const makeBinding = callback => (...args) => {
      if (args.length !== 1 || typeof args[0] !== 'string') {
        throw new Error('Invalid arguments: should be exactly one string.');
      }
      callback(args[0]);
    };
    window.bidiHostSendMessage = makeBinding(message => {
      fake.forHost.push(message);
    });
    window.bidiHostEndSession = makeBinding(() => {
      fake.forHost.push($1);
    });
)";

// Stands in for the mapper, which takes commands through this function.
constexpr char kFakeMapperScript[] = R"(
    window.onBidiMessage = message => {
      fake.forMapper.push(message);
    };
)";

constexpr char kSessionNew[] =
    R"({"id":1,"method":"session.new","params":{"capabilities":{}}})";

// What the mapper answers to `kSessionNew`.
constexpr char kSessionCreated[] =
    R"({"type":"success","id":1,"result":{"sessionId":"unknown"}})";

// The response to `kSessionNew` once the mapper cannot create a session.
constexpr char kNewConnectionRequired[] =
    R"({"type":"error","id":1,"error":"session not created",
        "message":"A new session requires a new connection"})";

constexpr char kSessionStatus[] =
    R"({"id":7,"method":"session.status","params":{}})";

// The responses to `kSessionStatus`.
constexpr char kReady[] =
    R"({"type":"success","id":7,
        "result":{"ready":true,"message":"ready for a new session"}})";
constexpr char kNotReady[] =
    R"({"type":"success","id":7,
        "result":{"ready":false,
                  "message":"a new session requires a new connection"}})";

constexpr char kGetTree[] =
    R"({"id":7,"method":"browsingContext.getTree","params":{}})";

// The response to a command with the id 7 that needs a session.
constexpr char kNoActiveSession[] =
    R"({"type":"error","id":7,"error":"invalid session id",
        "message":"No active session"})";

// Returns the "invalid argument" error for the command with the given id,
// which is written the way it is in JSON.
std::string InvalidArgument(std::string_view id, std::string_view message) {
  return base::StrCat({R"({"type":"error","id":)", id,
                       R"(,"error":"invalid argument","message":")", message,
                       R"("})"});
}

// Returns the error that tells that the mapper does not take the command with
// the given id.
std::string MapperNotRunning(std::string_view id) {
  return base::StrCat({R"({"type":"error","id":)", id,
                       R"(,"error":"unknown error",)",
                       R"("message":"The BiDi mapper is not running"})"});
}

// Returns a command with the id 7, the given method and no parameters.
std::string Command(std::string_view method) {
  return base::StrCat({R"({"id":7,"method":")", method, R"(","params":{}})"});
}

// Returns a command with the id 7 and the given method that takes `events`,
// a JSON list.
std::string EventsCommand(std::string_view method, std::string_view events) {
  return base::StrCat({R"({"id":7,"method":")", method,
                       R"(","params":{"events":)", events, "}}"});
}

// Returns the strings that `list` holds.
std::vector<std::string> ToStrings(const base::ListValue* list) {
  std::vector<std::string> strings;
  if (!list) {
    ADD_FAILURE() << "Not a list";
    return strings;
  }
  for (const base::Value& value : *list) {
    if (const std::string* text = value.GetIfString()) {
      strings.push_back(*text);
    } else {
      ADD_FAILURE() << "Not a string: " << value;
    }
  }
  return strings;
}

// Tests the script that stands between a BidiMapperHost and its mapper on its
// own, with stand-ins for the host and for the mapper. The tests of the host
// cover how the script works with the real ones.
class BidiMessageFilterBrowserTest : public content::ContentBrowserTest {
 protected:
  // What the filter did with a message.
  struct Outcome {
    // What the filter sent to the host, in order: messages for the client,
    // and `kSessionEnded`.
    std::vector<std::string> for_host;
    // The commands that the filter passed on to the mapper.
    std::vector<std::string> for_mapper;
  };

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    base::FilePath pak_dir;
#if BUILDFLAG(IS_MAC)
    ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &pak_dir));
#else
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_dir));
#endif
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_dir.Append(FILE_PATH_LITERAL("components_tests_resources.pak")),
        ui::kScaleFactorNone);

    ASSERT_TRUE(embedded_test_server()->Start());
    // Like the mapper document, this one is a secure context.
    ASSERT_TRUE(content::NavigateToURL(
        shell(), embedded_test_server()->GetURL("/title1.html")));
    // The mapper is evaluated after the filter, see BidiMapperHostImpl.
    ASSERT_TRUE(LoadFakeHost());
    ASSERT_TRUE(LoadFilter());
    ASSERT_TRUE(content::ExecJs(shell(), kFakeMapperScript));
  }

  testing::AssertionResult LoadFakeHost() {
    return content::ExecJs(shell(),
                           content::JsReplace(kFakeHostScript, kSessionEnded));
  }

  // Evaluates the filter in the current document, the way the host does in
  // the mapper document.
  testing::AssertionResult LoadFilter() {
    const std::string filter =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_BIDI_MESSAGE_FILTER_JS);
    if (filter.empty()) {
      return testing::AssertionFailure() << "The filter is missing";
    }
    return content::ExecJs(shell(), filter);
  }

  // Gives the filter a message of the client, the way the host does.
  Outcome SendFromClient(std::string_view message) {
    return Run(content::JsReplace("bidiHostOnMessage($1)", message));
  }

  // Gives the filter a message of the mapper, the way the mapper does.
  Outcome SendFromMapper(std::string_view message) {
    return Run(content::JsReplace("sendBidiResponse($1)", message));
  }

  // Gives the filter a message of the client that it is expected to answer
  // itself. Returns the response.
  std::string Respond(std::string_view message) {
    Outcome outcome = SendFromClient(message);
    EXPECT_THAT(outcome.for_mapper, IsEmpty()) << message;
    EXPECT_THAT(outcome.for_host, SizeIs(1)) << message;
    return outcome.for_host.empty() ? std::string() : outcome.for_host[0];
  }

  // Gives the filter a message of the client that it is expected to pass on.
  // Returns what the mapper gets.
  std::string PassOn(std::string_view message) {
    Outcome outcome = SendFromClient(message);
    EXPECT_THAT(outcome.for_host, IsEmpty()) << message;
    EXPECT_THAT(outcome.for_mapper, SizeIs(1)) << message;
    return outcome.for_mapper.empty() ? std::string() : outcome.for_mapper[0];
  }

  // Gives the filter a message of the mapper that it is expected to pass on
  // as it is.
  void ExpectUnchanged(const std::string& message) {
    EXPECT_THAT(SendFromMapper(message).for_host, ElementsAre(message));
  }

  // Gives the filter a message of the mapper that it is expected to drop.
  void ExpectDropped(const std::string& message) {
    EXPECT_THAT(SendFromMapper(message).for_host, IsEmpty()) << message;
  }

  void CreateSession() {
    PassOn(kSessionNew);
    EXPECT_THAT(SendFromMapper(kSessionCreated).for_host, SizeIs(1));
  }

 private:
  // Runs `script`, which gives the filter a message. Returns what the filter
  // did with the message.
  Outcome Run(const std::string& script) {
    Outcome outcome;
    content::EvalJsResult result =
        content::EvalJs(shell(), base::StrCat({script, "; fake.take()"}));
    if (!result.is_dict()) {
      ADD_FAILURE() << script << ": " << result;
      return outcome;
    }
    outcome.for_host = ToStrings(result.ExtractDict().FindList("forHost"));
    outcome.for_mapper = ToStrings(result.ExtractDict().FindList("forMapper"));
    return outcome;
  }
};

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, RejectsMalformedCommands) {
  EXPECT_THAT(Respond("session.status"),
              IsJson(InvalidArgument("null", "Cannot parse data as JSON")));
  EXPECT_THAT(
      Respond("[]"),
      IsJson(InvalidArgument("null", "Expected JSON object but got array")));

  // The id has to be a non-negative safe integer (WebDriver BiDi js-uint).
  EXPECT_THAT(Respond(R"({"method":"session.status","params":{}})"),
              IsJson(InvalidArgument(
                  "null", "Expected unsigned integer but got undefined")));
  for (const char* id : {"-1", "1.5", "9007199254740992", "\"1\"", "null"}) {
    const std::string command = base::StrCat(
        {R"({"id":)", id, R"(,"method":"session.status","params":{}})"});
    base::DictValue response = ParseJsonDict(Respond(command));
    EXPECT_THAT(response,
                DictionaryHasValue("error", base::Value("invalid argument")))
        << command;
    EXPECT_THAT(response, DictionaryHasValue("id", base::Value())) << command;
  }
  // 2^53 - 1 (MAX_SAFE_INTEGER) does not fit in an int, and is valid.
  base::DictValue response = ParseJsonDict(Respond(
      R"({"id":9007199254740991,"method":"session.status","params":{}})"));
  EXPECT_EQ(response.FindDouble("id"), 9007199254740991.0);
  EXPECT_THAT(response, DictionaryHasValue("type", base::Value("success")));

  EXPECT_THAT(
      Respond(R"({"id":2,"params":{}})"),
      IsJson(InvalidArgument("2", "Expected string method but got undefined")));
  EXPECT_THAT(
      Respond(R"({"id":3,"method":"session.status"})"),
      IsJson(InvalidArgument("3", "Expected object params but got undefined")));
  EXPECT_THAT(
      Respond(R"({"id":4,"method":"session.status","params":[]})"),
      IsJson(InvalidArgument("4", "Expected object params but got array")));
  EXPECT_THAT(
      Respond(
          R"({"id":5,"method":"session.status","params":{},"goog:channel":1})"),
      IsJson(InvalidArgument("5", "Expected string channel but got number")));
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, SessionStatus) {
  EXPECT_THAT(Respond(kSessionStatus), IsJson(kReady));

  CreateSession();
  EXPECT_THAT(Respond(kSessionStatus), IsJson(kNotReady));
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, SessionNew) {
  constexpr char kCommand[] =
      R"({"id":1,"method":"session.new",
          "params":{"capabilities":{"alwaysMatch":{"a":1},
                                    "firstMatch":[{"b":2}]}}})";
  EXPECT_EQ(PassOn(kCommand), kCommand);

  // Events that the mapper sends in the meantime are not affected.
  ExpectUnchanged(R"({"type":"event","method":"log.entryAdded","params":{}})");

  // The mapper does not know the id of the session.
  Outcome outcome = SendFromMapper(
      R"({"type":"success","id":1,
          "result":{"sessionId":"unknown","capabilities":{"a":1}}})");
  ASSERT_THAT(outcome.for_host, SizeIs(1));
  base::DictValue response = ParseJsonDict(outcome.for_host[0]);
  const std::string* session_id =
      response.FindStringByDottedPath("result.sessionId");
  ASSERT_TRUE(session_id);
  EXPECT_TRUE(base::Uuid::ParseLowercase(*session_id).is_valid())
      << *session_id;
  response.FindDict("result")->Remove("sessionId");
  EXPECT_THAT(
      response,
      IsJson(R"({"type":"success","id":1,"result":{"capabilities":{"a":1}}})"));

  // Later responses with the same id are not touched.
  ExpectUnchanged(kSessionCreated);
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest,
                       SessionNewChecksCapabilities) {
  constexpr const char* kInvalidParams[] = {
      R"({})",
      R"({"capabilities":1})",
      R"({"capabilities":{"alwaysMatch":[]}})",
      R"({"capabilities":{"firstMatch":{}}})",
      R"({"capabilities":{"firstMatch":[{},1]}})",
  };
  for (const char* params : kInvalidParams) {
    const std::string command = base::StrCat(
        {R"({"id":7,"method":"session.new","params":)", params, "}"});
    EXPECT_THAT(ParseJsonDict(Respond(command)),
                DictionaryHasValue("error", base::Value("invalid argument")))
        << command;
  }

  // A session can still be created.
  PassOn(kSessionNew);
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, SessionNewTwice) {
  constexpr char kSessionExists[] =
      R"({"type":"error","id":1,"error":"session not created",
          "message":"Session already exists"})";

  PassOn(kSessionNew);
  // The mapper has not answered yet.
  EXPECT_THAT(Respond(kSessionNew), IsJson(kSessionExists));

  EXPECT_THAT(SendFromMapper(kSessionCreated).for_host, SizeIs(1));
  EXPECT_THAT(Respond(kSessionNew), IsJson(kSessionExists));
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, SessionNewFails) {
  PassOn(kSessionNew);
  ExpectUnchanged(
      R"({"type":"error","id":1,"error":"invalid argument","message":"m"})");

  // The mapper cannot create another session.
  EXPECT_THAT(Respond(kSessionNew), IsJson(kNewConnectionRequired));
  EXPECT_THAT(Respond(kSessionStatus), IsJson(kNotReady));
  EXPECT_THAT(Respond(kGetTree), IsJson(kNoActiveSession));
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, CommandsRequireSession) {
  EXPECT_THAT(Respond(kGetTree), IsJson(kNoActiveSession));

  // The session does not exist until the mapper answers.
  PassOn(kSessionNew);
  EXPECT_THAT(Respond(kGetTree), IsJson(kNoActiveSession));

  EXPECT_THAT(SendFromMapper(kSessionCreated).for_host, SizeIs(1));
  EXPECT_EQ(PassOn(kGetTree), kGetTree);
  ExpectUnchanged(R"({"type":"success","id":7,"result":{"contexts":[]}})");
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, SessionEnd) {
  const std::string session_end = Command("session.end");
  EXPECT_THAT(Respond(session_end), IsJson(kNoActiveSession));

  CreateSession();
  Outcome outcome = SendFromClient(session_end);
  EXPECT_THAT(outcome.for_mapper, IsEmpty());
  // The host gets the response first: it passes nothing on once the session
  // ended.
  EXPECT_THAT(outcome.for_host,
              ElementsAre(IsJson(R"({"type":"success","id":7,"result":{}})"),
                          kSessionEnded));

  // The filter does not serve another session.
  EXPECT_THAT(Respond(session_end), IsJson(kNoActiveSession));
  EXPECT_THAT(Respond(kSessionNew), IsJson(kNewConnectionRequired));
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, RefusesCdpCommands) {
  constexpr char kSendCommand[] =
      R"({"id":7,"method":"goog:cdp.sendCommand",
          "params":{"method":"Browser.getVersion","params":{}}})";
  constexpr char kUnknownCommand[] =
      R"({"type":"error","id":7,"error":"unknown command",
          "message":"Unknown command 'goog:cdp.sendCommand'."})";
  EXPECT_THAT(Respond(kSendCommand), IsJson(kUnknownCommand));

  CreateSession();
  EXPECT_THAT(Respond(kSendCommand), IsJson(kUnknownCommand));
  for (const char* method : {"goog:cdp.getSession", "goog:cdp.resolveRealm",
                             "goog:cdp", "goog:cdpX.y"}) {
    EXPECT_THAT(ParseJsonDict(Respond(Command(method))),
                DictionaryHasValue("error", base::Value("unknown command")))
        << method;
  }
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest,
                       RefusesCdpEventSubscriptions) {
  CreateSession();

  const std::string subscribe =
      EventsCommand("session.subscribe", R"(["browsingContext.load","log"])");
  EXPECT_EQ(PassOn(subscribe), subscribe);

  EXPECT_THAT(Respond(EventsCommand("session.subscribe", R"(["goog:cdp"])")),
              IsJson(InvalidArgument("7", "Unknown event: goog:cdp")));
  EXPECT_THAT(
      Respond(EventsCommand(
          "session.subscribe",
          R"(["browsingContext.load","goog:cdp.Network.requestWillBeSent"])")),
      IsJson(InvalidArgument(
          "7", "Unknown event: goog:cdp.Network.requestWillBeSent")));
  EXPECT_THAT(Respond(EventsCommand("session.unsubscribe",
                                    R"(["goog:cdp.Page.frameNavigated"])")),
              IsJson(InvalidArgument(
                  "7", "Unknown event: goog:cdp.Page.frameNavigated")));
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, DropsCdpEvents) {
  CreateSession();

  ExpectDropped(R"({"type":"event","method":"goog:cdp.Page.frameNavigated",
                    "params":{"event":"Page.frameNavigated","params":{}}})");

  // The name of the module is not special anywhere else in a message.
  ExpectUnchanged(R"({"type":"event","method":"log.entryAdded",
                      "params":{"text":"goog:cdp.Page.frameNavigated"}})");
  ExpectUnchanged(R"({"type":"success","id":7,
                      "result":{"method":"goog:cdp.sendCommand"}})");

  // A message that cannot be inspected is only let through if it cannot be an
  // event of the module.
  ExpectDropped("goog:cdp");
  ExpectUnchanged("not JSON");
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest,
                       RefusesUnsupportedCommands) {
  constexpr char kEvaluate[] =
      R"({"id":7,"method":"script.evaluate",
          "params":{"expression":"1","awaitPromise":false,
                    "target":{"context":"c"}}})";
  constexpr char kEvaluateRefused[] =
      R"({"type":"error","id":7,"error":"unsupported operation",
          "message":"Method script.evaluate is not supported."})";
  constexpr char kCallFunction[] =
      R"({"id":7,"method":"script.callFunction","params":{},
          "goog:channel":"a"})";
  constexpr char kCallFunctionRefused[] =
      R"({"type":"error","id":7,"goog:channel":"a",
          "error":"unsupported operation",
          "message":"Method script.callFunction is not supported."})";

  // The commands need a session, like any other.
  EXPECT_THAT(Respond(kEvaluate), IsJson(kNoActiveSession));

  CreateSession();
  EXPECT_THAT(Respond(kEvaluate), IsJson(kEvaluateRefused));
  EXPECT_THAT(Respond(kCallFunction), IsJson(kCallFunctionRefused));

  // The filter refuses these commands, and not their module.
  const std::string get_realms = Command("script.getRealms");
  EXPECT_EQ(PassOn(get_realms), get_realms);
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, EchoesChannel) {
  constexpr char kStatus[] =
      R"({"id":7,"method":"session.status","params":{},"goog:channel":"a"})";
  constexpr char kStatusResponse[] =
      R"({"type":"success","id":7,"goog:channel":"a",
          "result":{"ready":true,"message":"ready for a new session"}})";
  EXPECT_THAT(Respond(kStatus), IsJson(kStatusResponse));

  constexpr char kGetSession[] =
      R"({"id":7,"method":"goog:cdp.getSession","params":{},
          "goog:channel":"a"})";
  constexpr char kGetSessionResponse[] =
      R"({"type":"error","id":7,"goog:channel":"a","error":"unknown command",
          "message":"Unknown command 'goog:cdp.getSession'."})";
  EXPECT_THAT(Respond(kGetSession), IsJson(kGetSessionResponse));

  // An empty channel is no channel.
  constexpr char kGetTreeWithEmptyChannel[] =
      R"({"id":7,"method":"browsingContext.getTree","params":{},
          "goog:channel":""})";
  EXPECT_THAT(Respond(kGetTreeWithEmptyChannel), IsJson(kNoActiveSession));
}

// The mapper must act on the command that the filter checked. Commands that
// can be read in two ways are a way to get around that.
IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest,
                       PassesOnTheCommandItChecked) {
  CreateSession();

  constexpr const char* kAmbiguousCommands[] = {
      R"({"id":7,"method":"browsingContext.getTree",
          "method":"goog:cdp.sendCommand","params":{}})",
      R"({"id":7,"method":"goog:cdp.sendCommand",
          "method":"browsingContext.getTree","params":{}})",
      R"({"id":7,"method":"session.subscribe","params":{"events":["log"]},
          "params":{"events":["goog:cdp"]}})",
      R"({"id":7,"method":"session.subscribe","params":{"events":["goog:cdp"]},
          "params":{"events":["log"]}})",
      R"({"id":7,"method":"browsingContext.getTree",
          "method":"script.evaluate","params":{}})",
      R"({"id":7,"method":"script.evaluate",
          "method":"browsingContext.getTree","params":{}})",
  };
  for (const char* command : kAmbiguousCommands) {
    Outcome outcome = SendFromClient(command);
    if (outcome.for_mapper.empty()) {
      EXPECT_THAT(outcome.for_host, SizeIs(1)) << command;
      continue;
    }
    EXPECT_THAT(outcome.for_host, IsEmpty()) << command;
    // The mapper gets the command as the client sent it, and reads it the way
    // the filter did: with no trace of what the filter refuses.
    EXPECT_THAT(outcome.for_mapper, ElementsAre(command));
    EXPECT_THAT(
        content::EvalJs(shell(), content::JsReplace(
                                     "JSON.stringify(JSON.parse($1))", command))
            .ExtractString(),
        AllOf(Not(HasSubstr("goog:cdp")), Not(HasSubstr("script.evaluate"))))
        << command;
  }
}

// The mapper stops taking commands when it closes its transport.
IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, MapperNotRunning) {
  CreateSession();
  ASSERT_TRUE(content::ExecJs(shell(), "window.onBidiMessage = null"));

  EXPECT_THAT(Respond(kGetTree), IsJson(MapperNotRunning("7")));
  // The filter still answers the commands that it handles itself.
  EXPECT_THAT(Respond(kSessionStatus), IsJson(kNotReady));
}

IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest,
                       SessionNewWhenMapperNotRunning) {
  ASSERT_TRUE(content::ExecJs(shell(), "window.onBidiMessage = null"));

  EXPECT_THAT(Respond(kSessionNew), IsJson(MapperNotRunning("1")));
  EXPECT_THAT(Respond(kSessionNew), IsJson(kNewConnectionRequired));
  EXPECT_THAT(Respond(kSessionStatus), IsJson(kNotReady));
}

// The host does not check that it added each of its bindings. It relies on
// the filter, which does not load without them.
IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, RequiresBindings) {
  // A new document has neither the bindings nor the filter.
  ASSERT_TRUE(content::NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title2.html")));
  for (const char* binding : {"bidiHostSendMessage", "bidiHostEndSession"}) {
    ASSERT_TRUE(LoadFakeHost());
    ASSERT_TRUE(content::ExecJs(
        shell(), content::JsReplace("delete window[$1]", binding)));
    EXPECT_FALSE(LoadFilter()) << binding;
    EXPECT_EQ("undefined",
              content::EvalJs(shell(), "typeof window.bidiHostOnMessage"))
        << binding;
    EXPECT_EQ("undefined",
              content::EvalJs(shell(), "typeof window.sendBidiResponse"))
        << binding;
  }
}

// Every message passes through the filter. The scripts that run in the
// document after it, the mapper among them, have no way around it.
IN_PROC_BROWSER_TEST_F(BidiMessageFilterBrowserTest, CannotBeBypassed) {
  // The bindings of the host are gone.
  EXPECT_EQ("undefined",
            content::EvalJs(shell(), "typeof window.bidiHostSendMessage"));
  EXPECT_EQ("undefined",
            content::EvalJs(shell(), "typeof window.bidiHostEndSession"));

  // The functions of the filter cannot be replaced.
  constexpr char kReplaceScript[] = R"(
      (() => {
        const name = $1;
        const original = window[name];
        const replacement = () => {};
        window[name] = replacement;
        delete window[name];
        try {
          Object.defineProperty(window, name, {value: replacement});
        } catch {
        }
        return typeof original === 'function' && window[name] === original;
      })()
  )";
  for (const char* name : {"bidiHostOnMessage", "sendBidiResponse"}) {
    EXPECT_EQ(true, content::EvalJs(shell(),
                                    content::JsReplace(kReplaceScript, name)))
        << name;
  }

  // The filter works as before.
  CreateSession();
  EXPECT_THAT(ParseJsonDict(Respond(Command("goog:cdp.sendCommand"))),
              DictionaryHasValue("error", base::Value("unknown command")));
  ExpectDropped(R"({"type":"event","method":"goog:cdp.Page.frameNavigated",
                    "params":{}})");
}

}  // namespace

}  // namespace bidi_mapper
