// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SessionCommandsTest, ExecuteGetTimeouts) {
  Session session("id");
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  Status status = ExecuteGetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());
  base::Value::Dict* response = value->GetIfDict();
  ASSERT_TRUE(response);

  int script = response->FindInt("script").value_or(-1);
  ASSERT_EQ(script, 30000);
  int page_load = response->FindInt("pageLoad").value_or(-1);
  ASSERT_EQ(page_load, 300000);
  int implicit = response->FindInt("implicit").value_or(-1);
  ASSERT_EQ(implicit, 0);
}

TEST(SessionCommandsTest, ExecuteSetTimeouts) {
  Session session("id");
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  // W3C spec doesn't forbid passing in an empty object, so we should get kOk.
  Status status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());

  params.Set("pageLoad", 5000);
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());

  params.Set("script", 5000);
  params.Set("implicit", 5000);
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());

  params.Set("implicit", -5000);
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kInvalidArgument, status.code());

  params.clear();
  params.Set("unknown", 5000);
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());

  // Old pre-W3C format.
  params.clear();
  params.Set("ms", 5000.0);
  params.Set("type", "page load");
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());
}

TEST(SessionCommandsTest, MergeCapabilities) {
  base::Value::Dict primary;
  primary.Set("strawberry", "velociraptor");
  primary.Set("pear", "unicorn");

  base::Value::Dict secondary;
  secondary.Set("broccoli", "giraffe");
  secondary.Set("celery", "hippo");
  secondary.Set("eggplant", "elephant");

  base::Value::Dict merged;

  // key collision should return false
  ASSERT_FALSE(MergeCapabilities(primary, primary, merged));
  // non key collision should return true
  ASSERT_TRUE(MergeCapabilities(primary, secondary, merged));

  merged.clear();
  MergeCapabilities(primary, secondary, merged);
  primary.Merge(std::move(secondary));

  ASSERT_EQ(primary, merged);
}

TEST(SessionCommandsTest, ProcessCapabilities_Empty) {
  // "capabilities" is required
  base::Value::Dict params;
  base::Value::Dict result;
  Status status = ProcessCapabilities(params, result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // "capabilities" must be a JSON object
  params.Set("capabilities", base::Value::List());
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Empty "capabilities" is OK
  params.Set("capabilities", base::Value::Dict());
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(result.empty());
}

TEST(SessionCommandsTest, ProcessCapabilities_AlwaysMatch) {
  base::Value::Dict params;
  base::Value::Dict result;

  // "alwaysMatch" must be a JSON object
  params.SetByDottedPath("capabilities.alwaysMatch", base::Value::List());
  Status status = ProcessCapabilities(params, result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Empty "alwaysMatch" is OK
  params.SetByDottedPath("capabilities.alwaysMatch", base::Value::Dict());
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(result.empty());

  // Invalid "alwaysMatch"
  params.SetByDottedPath("capabilities.alwaysMatch.browserName", 10);
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Valid "alwaysMatch"
  params.SetByDottedPath("capabilities.alwaysMatch.browserName", "chrome");
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 1u);
  std::string* result_string = result.FindString("browserName");
  ASSERT_TRUE(result_string);
  ASSERT_EQ(*result_string, "chrome");

  // Null "browserName" treated as not specifying "browserName"
  params.SetByDottedPath("capabilities.alwaysMatch.browserName", base::Value());
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_FALSE(result.FindString("browserName"));
}

TEST(SessionCommandsTest, ProcessCapabilities_FirstMatch) {
  base::Value::Dict params;
  base::Value::Dict result;

  // "firstMatch" must be a JSON list
  params.SetByDottedPath("capabilities.firstMatch", base::Value::Dict());
  Status status = ProcessCapabilities(params, result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // "firstMatch" must have at least one entry
  params.SetByDottedPath("capabilities.firstMatch",
                         base::Value(base::Value::Type::LIST));
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Each entry must be a JSON object
  base::Value::List* list =
      params.FindListByDottedPath("capabilities.firstMatch");
  list->Append(base::Value::List());
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Empty JSON object allowed as an entry
  (*list)[0] = base::Value(base::Value::Type::DICT);
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(result.empty());

  // Invalid entry
  base::Value::Dict* entry = (*list)[0].GetIfDict();
  entry->Set("pageLoadStrategy", "invalid");
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Valid entry
  entry->Set("pageLoadStrategy", "eager");
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 1u);
  std::string* result_string = result.FindString("pageLoadStrategy");
  ASSERT_TRUE(result_string);
  ASSERT_EQ(*result_string, "eager");

  // Multiple entries, the first one should be selected.
  list->Append(base::Value::Dict());
  entry = (*list)[1].GetIfDict();
  entry->Set("pageLoadStrategy", "normal");
  entry->Set("browserName", "chrome");
  status = ProcessCapabilities(params, result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 1u);
  result_string = result.FindString("pageLoadStrategy");
  ASSERT_TRUE(result_string);
  ASSERT_EQ(*result_string, "eager");
}

namespace {

Status ProcessCapabilitiesJson(const std::string& params_json,
                               base::Value::Dict& result_capabilities) {
  base::Value::Dict params = base::test::ParseJsonDict(params_json);
  return ProcessCapabilities(params, result_capabilities);
}

}  // namespace

TEST(SessionCommandsTest, ProcessCapabilities_Merge) {
  base::Value::Dict result;
  Status status(kOk);

  // Disallow setting same capability in alwaysMatch and firstMatch
  status = ProcessCapabilitiesJson(
      R"({
        "capabilities": {
          "alwaysMatch": { "pageLoadStrategy": "normal" },
          "firstMatch": [
            { "unhandledPromptBehavior": "accept" },
            { "pageLoadStrategy": "normal" }
          ]
        }
      })",
      result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // No conflicts between alwaysMatch and firstMatch, select first firstMatch
  status = ProcessCapabilitiesJson(
      R"({
        "capabilities": {
          "alwaysMatch": { "timeouts": { } },
          "firstMatch": [
            { "unhandledPromptBehavior": "accept" },
            { "pageLoadStrategy": "normal" }
          ]
        }
      })",
      result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 2u);
  ASSERT_TRUE(result.Find("timeouts"));
  ASSERT_TRUE(result.Find("unhandledPromptBehavior"));
  ASSERT_FALSE(result.Find("pageLoadStrategy"));

  // Selection by platformName
  std::string platform_name =
      base::ToLowerASCII(base::SysInfo::OperatingSystemName());
  status = ProcessCapabilitiesJson(
      R"({
       "capabilities": {
         "alwaysMatch": { "timeouts": { "script": 10 } },
         "firstMatch": [
           { "platformName": "LINUX", "pageLoadStrategy": "none" },
           { "platformName": ")" +
          platform_name + R"(", "pageLoadStrategy": "eager" }
         ]
       }
     })",
      result);
  printf("THIS IS PLATFORM: %s", platform_name.c_str());
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(*result.FindString("platformName"), platform_name);
  ASSERT_EQ(*result.FindString("pageLoadStrategy"), "eager");

  // Selection by browserName
  status = ProcessCapabilitiesJson(
      R"({
        "capabilities": {
          "alwaysMatch": { "timeouts": { } },
          "firstMatch": [
            { "browserName": "firefox", "unhandledPromptBehavior": "accept" },
            { "browserName": "chrome", "pageLoadStrategy": "normal" }
          ]
        }
      })",
      result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 3u);
  ASSERT_TRUE(result.Find("timeouts"));
  ASSERT_EQ(*result.FindString("browserName"), "chrome");
  ASSERT_FALSE(result.Find("unhandledPromptBehavior"));
  ASSERT_TRUE(result.Find("pageLoadStrategy"));

  // No acceptable firstMatch
  status = ProcessCapabilitiesJson(
      R"({
        "capabilities": {
          "alwaysMatch": { "timeouts": { } },
          "firstMatch": [
            { "browserName": "firefox", "unhandledPromptBehavior": "accept" },
            { "browserName": "edge", "pageLoadStrategy": "normal" }
          ]
        }
      })",
      result);
  ASSERT_EQ(kSessionNotCreated, status.code());
}

TEST(SessionCommandsTest, FileUpload) {
  Session session("id");
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;
  // Zip file entry that contains a single file with contents 'COW\n', base64
  // encoded following RFC 1521.
  const char kBase64ZipEntry[] =
      "UEsDBBQAAAAAAMROi0K/wAzGBAAAAAQAAAADAAAAbW9vQ09XClBLAQIUAxQAAAAAAMROi0K/"
      "wAzG\nBAAAAAQAAAADAAAAAAAAAAAAAACggQAAAABtb29QSwUGAAAAAAEAAQAxAAAAJQAAAA"
      "AA\n";
  params.Set("file", kBase64ZipEntry);
  Status status = ExecuteUploadFile(&session, params, &value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(value->is_string());
  std::string path = value->GetString();
  ASSERT_TRUE(base::PathExists(base::FilePath::FromUTF8Unsafe(path)));
  std::string data;
  ASSERT_TRUE(
      base::ReadFileToString(base::FilePath::FromUTF8Unsafe(path), &data));
  ASSERT_STREQ("COW\n", data.c_str());
}

namespace {

class DetachChrome : public StubChrome {
 public:
  DetachChrome() : quit_called_(false) {}
  ~DetachChrome() override = default;

  // Overridden from Chrome:
  Status Quit() override {
    quit_called_ = true;
    return Status(kOk);
  }

  bool quit_called_;
};

}  // namespace

TEST(SessionCommandsTest, MatchCapabilities) {
  base::Value::Dict merged;
  merged.Set("browserName", "not chrome");

  ASSERT_FALSE(MatchCapabilities(merged));

  merged.clear();
  merged.Set("browserName", "chrome");

  ASSERT_TRUE(MatchCapabilities(merged));
}

TEST(SessionCommandsTest, MatchCapabilitiesVirtualAuthenticators) {
  // Match webauthn:virtualAuthenticators on desktop.
  base::Value::Dict merged;
  merged.SetByDottedPath("webauthn:virtualAuthenticators", true);
  EXPECT_TRUE(MatchCapabilities(merged));

  // Don't match webauthn:virtualAuthenticators on android.
  merged.SetByDottedPath("goog:chromeOptions.androidPackage", "packageName");
  EXPECT_FALSE(MatchCapabilities(merged));

  // Don't match values other than bools.
  merged.clear();
  merged.Set("webauthn:virtualAuthenticators", "not a bool");
  EXPECT_FALSE(MatchCapabilities(merged));
}

TEST(SessionCommandsTest, MatchCapabilitiesVirtualAuthenticatorsLargeBlob) {
  // Match webauthn:extension:largeBlob on desktop.
  base::Value::Dict merged;
  merged.SetByDottedPath("webauthn:extension:largeBlob", true);
  EXPECT_TRUE(MatchCapabilities(merged));

  // Don't match webauthn:extension:largeBlob on android.
  merged.SetByDottedPath("goog:chromeOptions.androidPackage", "packageName");
  EXPECT_FALSE(MatchCapabilities(merged));

  // Don't match values other than bools.
  merged.clear();
  merged.Set("webauthn:extension:largeBlob", "not a bool");
  EXPECT_FALSE(MatchCapabilities(merged));
}

TEST(SessionCommandsTest, MatchCapabilitiesFedCm) {
  // Match fedcm:accounts.
  base::Value::Dict merged;
  merged.SetByDottedPath("fedcm:accounts", true);
  EXPECT_TRUE(MatchCapabilities(merged));

  // Don't match false.
  merged.SetByDottedPath("fedcm:accounts", false);
  EXPECT_FALSE(MatchCapabilities(merged));

  // Don't match values other than bools.
  merged.clear();
  merged.Set("fedcm:accounts", "not a bool");
  EXPECT_FALSE(MatchCapabilities(merged));
}

TEST(SessionCommandsTest, Quit) {
  DetachChrome* chrome = new DetachChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));

  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  ASSERT_EQ(kOk, ExecuteQuit(false, &session, params, &value).code());
  ASSERT_TRUE(chrome->quit_called_);

  chrome->quit_called_ = false;
  ASSERT_EQ(kOk, ExecuteQuit(true, &session, params, &value).code());
  ASSERT_TRUE(chrome->quit_called_);
}

TEST(SessionCommandsTest, QuitWithDetach) {
  DetachChrome* chrome = new DetachChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));
  session.detach = true;

  base::Value::Dict params;
  std::unique_ptr<base::Value> value;

  ASSERT_EQ(kOk, ExecuteQuit(true, &session, params, &value).code());
  ASSERT_FALSE(chrome->quit_called_);

  ASSERT_EQ(kOk, ExecuteQuit(false, &session, params, &value).code());
  ASSERT_TRUE(chrome->quit_called_);
}

namespace {

class FailsToQuitChrome : public StubChrome {
 public:
  FailsToQuitChrome() = default;
  ~FailsToQuitChrome() override = default;

  // Overridden from Chrome:
  Status Quit() override { return Status(kUnknownError); }
};

}  // namespace

TEST(SessionCommandsTest, QuitFails) {
  Session session("id", std::unique_ptr<Chrome>(new FailsToQuitChrome()));
  base::Value::Dict params;
  std::unique_ptr<base::Value> value;
  ASSERT_EQ(kUnknownError, ExecuteQuit(false, &session, params, &value).code());
}

namespace {

class MockChrome : public StubChrome {
 public:
  explicit MockChrome(BrowserInfo& binfo) : web_view_("1") {
    browser_info_ = binfo;
  }
  ~MockChrome() override = default;

  const BrowserInfo* GetBrowserInfo() const override { return &browser_info_; }

  Status GetWebViewById(const std::string& id, WebView** web_view) override {
    *web_view = &web_view_;
    return Status(kOk);
  }

 private:
  BrowserInfo browser_info_;
  StubWebView web_view_;
};

}  // namespace

TEST(SessionCommandsTest, ConfigureHeadlessSession_dotNotation) {
  Capabilities capabilities;
  base::Value::Dict caps;
  base::Value::List args;
  args.Append("headless");
  caps.SetByDottedPath("goog:chromeOptions.args", base::Value(std::move(args)));
  caps.SetByDottedPath("goog:chromeOptions.prefs.download.default_directory",
                       "/examples/python/downloads");

  Status status = capabilities.Parse(caps);
  BrowserInfo binfo;
  binfo.is_headless_shell = true;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  status = internal::ConfigureHeadlessSession(&session, capabilities);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(session.chrome->GetBrowserInfo()->is_headless_shell);
  ASSERT_STREQ("/examples/python/downloads",
               session.headless_download_directory->c_str());
}

TEST(SessionCommandsTest, ConfigureHeadlessSession_nestedMap) {
  Capabilities capabilities;
  base::Value::Dict caps;
  base::Value::List args;
  args.Append("headless");
  caps.SetByDottedPath("goog:chromeOptions.args", base::Value(std::move(args)));
  caps.SetByDottedPath("goog:chromeOptions.prefs.download.default_directory",
                       "/examples/python/downloads");

  Status status = capabilities.Parse(caps);
  BrowserInfo binfo;
  binfo.is_headless_shell = true;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  status = internal::ConfigureHeadlessSession(&session, capabilities);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(session.chrome->GetBrowserInfo()->is_headless_shell);
  ASSERT_STREQ("/examples/python/downloads",
               session.headless_download_directory->c_str());
}

TEST(SessionCommandsTest, ConfigureHeadlessSession_noDownloadDir) {
  Capabilities capabilities;
  base::Value::Dict caps;
  base::Value::List args;
  args.Append("headless");
  caps.SetByDottedPath("goog:chromeOptions.args", base::Value(std::move(args)));

  Status status = capabilities.Parse(caps);
  BrowserInfo binfo;
  binfo.is_headless_shell = true;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  status = internal::ConfigureHeadlessSession(&session, capabilities);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(session.chrome->GetBrowserInfo()->is_headless_shell);
  ASSERT_STREQ(".", session.headless_download_directory->c_str());
}

TEST(SessionCommandsTest, ConfigureHeadlessSession_notHeadless) {
  Capabilities capabilities;
  base::Value::Dict caps;
  caps.SetByDottedPath("goog:chromeOptions.prefs.download.default_directory",
                       "/examples/python/downloads");

  Status status = capabilities.Parse(caps);
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  status = internal::ConfigureHeadlessSession(&session, capabilities);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_FALSE(session.chrome->GetBrowserInfo()->is_headless_shell);
  ASSERT_FALSE(session.headless_download_directory);
}

TEST(SessionCommandsTest, ConfigureSession_allSet) {
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  base::Value::Dict params_in = base::test::ParseJsonDict(
      R"({
        "capabilities": {
          "alwaysMatch": { },
          "firstMatch": [ {
            "acceptInsecureCerts": false,
            "browserName": "chrome",
            "goog:chromeOptions": {
            },
            "goog:loggingPrefs": {
              "driver": "DEBUG"
            },
            "pageLoadStrategy": "normal",
            "timeouts": {
              "implicit": 57000,
              "pageLoad": 29000,
              "script": 21000
            },
            "strictFileInteractability": true,
            "unhandledPromptBehavior": "accept"
          } ]
        }
      })");

  const base::Value::Dict* desired_caps_out = nullptr;
  base::Value::Dict merged_out;
  Capabilities capabilities_out;
  Status status = internal::ConfigureSession(
      &session, params_in, desired_caps_out, merged_out, &capabilities_out);
  ASSERT_EQ(kOk, status.code()) << status.message();
  // Verify out parameters have been set
  ASSERT_NE(desired_caps_out, nullptr);
  ASSERT_TRUE(capabilities_out.logging_prefs["driver"]);
  // Verify session settings are correct
  ASSERT_EQ(::prompt_behavior::kAccept,
            session.unhandled_prompt_behavior.CapabilityView().GetString());
  ASSERT_EQ(base::Seconds(57), session.implicit_wait);
  ASSERT_EQ(base::Seconds(29), session.page_load_timeout);
  ASSERT_EQ(base::Seconds(21), session.script_timeout);
  ASSERT_TRUE(session.strict_file_interactability);
  ASSERT_EQ(Log::Level::kDebug, session.driver_log.get()->min_level());
}

TEST(SessionCommandsTest, ConfigureSession_defaults) {
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  base::Value::Dict params_in = base::test::ParseJsonDict(
      R"({
        "capabilities": {
          "alwaysMatch": { },
          "firstMatch": [ { } ]
        }
      })");
  const base::Value::Dict* desired_caps_out = nullptr;
  base::Value::Dict merged_out;
  Capabilities capabilities_out;

  Status status = internal::ConfigureSession(
      &session, params_in, desired_caps_out, merged_out, &capabilities_out);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_NE(desired_caps_out, nullptr);
  // Testing specific values could be fragile, but want to verify they are set
  ASSERT_EQ(base::Seconds(0), session.implicit_wait);
  ASSERT_EQ(base::Seconds(300), session.page_load_timeout);
  ASSERT_EQ(base::Seconds(30), session.script_timeout);
  ASSERT_FALSE(session.strict_file_interactability);
  ASSERT_EQ(Log::Level::kWarning, session.driver_log.get()->min_level());
  // w3c values:
  ASSERT_EQ(::prompt_behavior::kDismissAndNotify,
            session.unhandled_prompt_behavior.CapabilityView().GetString());
}

TEST(SessionCommandsTest, ConfigureSession_legacyDefault) {
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  base::Value::Dict params_in = base::test::ParseJsonDict(
      R"({
        "desiredCapabilities": {
          "browserName": "chrome",
          "goog:chromeOptions": {
             "w3c": false
          }
        }
      })");
  const base::Value::Dict* desired_caps_out = nullptr;
  base::Value::Dict merged_out;
  Capabilities capabilities_out;

  Status status = internal::ConfigureSession(
      &session, params_in, desired_caps_out, merged_out, &capabilities_out);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_NE(desired_caps_out, nullptr);
  // legacy values:
  ASSERT_EQ(::prompt_behavior::kIgnore,
            session.unhandled_prompt_behavior.CapabilityView().GetString());
}

TEST(SessionCommandsTest, ConfigureSession_unhandledPromptBehaviorDict) {
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  base::Value::Dict params_in = base::test::ParseJsonDict(
      R"({
        "capabilities": {
          "alwaysMatch": {
            "unhandledPromptBehavior": {
              "alert": "accept",
              "confirm": "dismiss",
              "prompt": "ignore",
              "beforeUnload": "accept"
            }
          },
        }
      })");
  const base::Value::Dict* desired_caps_out = nullptr;
  base::Value::Dict merged_out;
  Capabilities capabilities_out;

  Status status = internal::ConfigureSession(
      &session, params_in, desired_caps_out, merged_out, &capabilities_out);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_NE(desired_caps_out, nullptr);
  // Testing specific values could be fragile, but want to verify they are set

  std::string json;
  base::JSONWriter::Write(session.unhandled_prompt_behavior.CapabilityView(),
                          &json);
  ASSERT_EQ(
      "{\"alert\":\"accept\",\"beforeUnload\":\"accept\",\"confirm\":"
      "\"dismiss\",\"prompt\":\"ignore\"}",
      json);
}
