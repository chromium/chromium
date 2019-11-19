// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
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
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;

  Status status = ExecuteGetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());
  base::DictionaryValue* response;
  ASSERT_TRUE(value->GetAsDictionary(&response));

  int script;
  ASSERT_TRUE(response->GetInteger("script", &script));
  ASSERT_EQ(script, 30000);
  int page_load;
  ASSERT_TRUE(response->GetInteger("pageLoad", &page_load));
  ASSERT_EQ(page_load, 300000);
  int implicit;
  ASSERT_TRUE(response->GetInteger("implicit", &implicit));
  ASSERT_EQ(implicit, 0);
}

TEST(SessionCommandsTest, ExecuteSetTimeouts) {
  Session session("id");
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;

  // W3C spec doesn't forbid passing in an empty object, so we should get kOk.
  Status status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());

  params.SetInteger("pageLoad", 5000);
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());

  params.SetInteger("script", 5000);
  params.SetInteger("implicit", 5000);
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());

  params.SetInteger("implicit", -5000);
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kInvalidArgument, status.code());

  params.Clear();
  params.SetInteger("unknown", 5000);
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());

  // Old pre-W3C format.
  params.Clear();
  params.SetDouble("ms", 5000.0);
  params.SetString("type", "page load");
  status = ExecuteSetTimeouts(&session, params, &value);
  ASSERT_EQ(kOk, status.code());
}

TEST(SessionCommandsTest, MergeCapabilities) {
  base::DictionaryValue primary;
  primary.SetString("strawberry", "velociraptor");
  primary.SetString("pear", "unicorn");

  base::DictionaryValue secondary;
  secondary.SetString("broccoli", "giraffe");
  secondary.SetString("celery", "hippo");
  secondary.SetString("eggplant", "elephant");

  base::DictionaryValue merged;

  // key collision should return false
  ASSERT_FALSE(MergeCapabilities(&primary, &primary, &merged));
  // non key collision should return true
  ASSERT_TRUE(MergeCapabilities(&primary, &secondary, &merged));

  merged.Clear();
  MergeCapabilities(&primary, &secondary, &merged);
  primary.MergeDictionary(&secondary);

  ASSERT_EQ(primary, merged);
}

TEST(SessionCommandsTest, ProcessCapabilities_Empty) {
  // "capabilities" is required
  base::DictionaryValue params;
  base::DictionaryValue result;
  Status status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // "capabilities" must be a JSON object
  params.SetList("capabilities", std::make_unique<base::ListValue>());
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Empty "capabilities" is OK
  params.SetDictionary("capabilities",
                       std::make_unique<base::DictionaryValue>());
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(result.empty());
}

TEST(SessionCommandsTest, ProcessCapabilities_AlwaysMatch) {
  base::DictionaryValue params;
  base::DictionaryValue result;

  // "alwaysMatch" must be a JSON object
  params.SetList("capabilities.alwaysMatch",
                 std::make_unique<base::ListValue>());
  Status status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Empty "alwaysMatch" is OK
  params.SetDictionary("capabilities.alwaysMatch",
                       std::make_unique<base::DictionaryValue>());
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(result.empty());

  // Invalid "alwaysMatch"
  params.SetInteger("capabilities.alwaysMatch.browserName", 10);
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Valid "alwaysMatch"
  params.SetString("capabilities.alwaysMatch.browserName", "chrome");
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 1u);
  std::string result_string;
  ASSERT_TRUE(result.GetString("browserName", &result_string));
  ASSERT_EQ(result_string, "chrome");

  // Null "browserName" treated as not specifying "browserName"
  params.SetPath({"capabilities", "alwaysMatch", "browserName"}, base::Value());
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_FALSE(result.GetString("browserName", &result_string));
}

TEST(SessionCommandsTest, ProcessCapabilities_FirstMatch) {
  base::DictionaryValue params;
  base::DictionaryValue result;

  // "firstMatch" must be a JSON list
  params.SetDictionary("capabilities.firstMatch",
                       std::make_unique<base::DictionaryValue>());
  Status status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // "firstMatch" must have at least one entry
  params.SetList("capabilities.firstMatch",
                 std::make_unique<base::ListValue>());
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Each entry must be a JSON object
  base::ListValue* list_ptr;
  ASSERT_TRUE(params.GetList("capabilities.firstMatch", &list_ptr));
  list_ptr->Set(0, std::make_unique<base::ListValue>());
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Empty JSON object allowed as an entry
  list_ptr->Set(0, std::make_unique<base::DictionaryValue>());
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(result.empty());

  // Invalid entry
  base::DictionaryValue* entry_ptr;
  ASSERT_TRUE(list_ptr->GetDictionary(0, &entry_ptr));
  entry_ptr->SetString("pageLoadStrategy", "invalid");
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kInvalidArgument, status.code());

  // Valid entry
  entry_ptr->SetString("pageLoadStrategy", "eager");
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 1u);
  std::string result_string;
  ASSERT_TRUE(result.GetString("pageLoadStrategy", &result_string));
  ASSERT_EQ(result_string, "eager");

  // Multiple entries, the first one should be selected.
  list_ptr->Set(1, std::make_unique<base::DictionaryValue>());
  ASSERT_TRUE(list_ptr->GetDictionary(1, &entry_ptr));
  entry_ptr->SetString("pageLoadStrategy", "normal");
  entry_ptr->SetString("browserName", "chrome");
  status = ProcessCapabilities(params, &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 1u);
  ASSERT_TRUE(result.GetString("pageLoadStrategy", &result_string));
  ASSERT_EQ(result_string, "eager");
}

namespace {

Status ProcessCapabilitiesJson(const std::string& paramsJson,
                               base::DictionaryValue* result_capabilities) {
  std::unique_ptr<base::Value> params =
      base::JSONReader::ReadDeprecated(paramsJson);
  if (!params || !params->is_dict())
    return Status(kUnknownError);
  return ProcessCapabilities(
      *static_cast<const base::DictionaryValue*>(params.get()),
      result_capabilities);
}

}  // namespace

TEST(SessionCommandsTest, ProcessCapabilities_Merge) {
  base::DictionaryValue result;
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
      &result);
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
      &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 2u);
  ASSERT_TRUE(result.HasKey("timeouts"));
  ASSERT_TRUE(result.HasKey("unhandledPromptBehavior"));
  ASSERT_FALSE(result.HasKey("pageLoadStrategy"));

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
      &result);
  printf("THIS IS PLATFORM: %s", platform_name.c_str());
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.FindKey("platformName")->GetString(), platform_name);
  ASSERT_EQ(result.FindKey("pageLoadStrategy")->GetString(), "eager");

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
      &result);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(result.size(), 3u);
  ASSERT_TRUE(result.HasKey("timeouts"));
  ASSERT_EQ(result.FindKey("browserName")->GetString(), "chrome");
  ASSERT_FALSE(result.HasKey("unhandledPromptBehavior"));
  ASSERT_TRUE(result.HasKey("pageLoadStrategy"));

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
      &result);
  ASSERT_EQ(kSessionNotCreated, status.code());
}

TEST(SessionCommandsTest, FileUpload) {
  Session session("id");
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  // Zip file entry that contains a single file with contents 'COW\n', base64
  // encoded following RFC 1521.
  const char kBase64ZipEntry[] =
      "UEsDBBQAAAAAAMROi0K/wAzGBAAAAAQAAAADAAAAbW9vQ09XClBLAQIUAxQAAAAAAMROi0K/"
      "wAzG\nBAAAAAQAAAADAAAAAAAAAAAAAACggQAAAABtb29QSwUGAAAAAAEAAQAxAAAAJQAAAA"
      "AA\n";
  params.SetString("file", kBase64ZipEntry);
  Status status = ExecuteUploadFile(&session, params, &value);
  ASSERT_EQ(kOk, status.code()) << status.message();
  base::FilePath::StringType path;
  ASSERT_TRUE(value->GetAsString(&path));
  ASSERT_TRUE(base::PathExists(base::FilePath(path)));
  std::string data;
  ASSERT_TRUE(base::ReadFileToString(base::FilePath(path), &data));
  ASSERT_STREQ("COW\n", data.c_str());
}

namespace {

class DetachChrome : public StubChrome {
 public:
  DetachChrome() : quit_called_(false) {}
  ~DetachChrome() override {}

  // Overridden from Chrome:
  Status Quit() override {
    quit_called_ = true;
    return Status(kOk);
  }

  bool quit_called_;
};

}  // namespace

TEST(SessionCommandsTest, MatchCapabilities) {
  base::DictionaryValue merged;
  merged.SetString("browserName", "not chrome");

  ASSERT_FALSE(MatchCapabilities(&merged));

  merged.Clear();
  merged.SetString("browserName", "chrome");

  ASSERT_TRUE(MatchCapabilities(&merged));
}

TEST(SessionCommandsTest, Quit) {
  DetachChrome* chrome = new DetachChrome();
  Session session("id", std::unique_ptr<Chrome>(chrome));

  base::DictionaryValue params;
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

  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;

  ASSERT_EQ(kOk, ExecuteQuit(true, &session, params, &value).code());
  ASSERT_FALSE(chrome->quit_called_);

  ASSERT_EQ(kOk, ExecuteQuit(false, &session, params, &value).code());
  ASSERT_TRUE(chrome->quit_called_);
}

namespace {

class FailsToQuitChrome : public StubChrome {
 public:
  FailsToQuitChrome() {}
  ~FailsToQuitChrome() override {}

  // Overridden from Chrome:
  Status Quit() override { return Status(kUnknownError); }
};

}  // namespace

TEST(SessionCommandsTest, QuitFails) {
  Session session("id", std::unique_ptr<Chrome>(new FailsToQuitChrome()));
  base::DictionaryValue params;
  std::unique_ptr<base::Value> value;
  ASSERT_EQ(kUnknownError, ExecuteQuit(false, &session, params, &value).code());
}

namespace {

class MockChrome : public StubChrome {
 public:
  explicit MockChrome(BrowserInfo& binfo) : web_view_("1") {
    browser_info_ = binfo;
  }
  ~MockChrome() override {}

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
  base::DictionaryValue caps;
  base::Value::ListStorage args;
  args.emplace_back("headless");
  caps.SetPath({"goog:chromeOptions", "args"}, base::Value(args));

  base::DictionaryValue prefs;
  prefs.SetKey("download.default_directory",
               base::Value("/examples/python/downloads"));
  caps.SetPath({"goog:chromeOptions", "prefs"}, prefs.Clone());

  Status status = capabilities.Parse(caps);
  BrowserInfo binfo;
  binfo.is_headless = true;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  status = internal::ConfigureHeadlessSession(&session, capabilities);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(session.chrome->GetBrowserInfo()->is_headless);
  ASSERT_STREQ("/examples/python/downloads",
               session.headless_download_directory->c_str());
}

TEST(SessionCommandsTest, ConfigureHeadlessSession_nestedMap) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  base::Value::ListStorage args;
  args.emplace_back("headless");
  caps.SetPath({"goog:chromeOptions", "args"}, base::Value(args));

  base::DictionaryValue prefs;
  std::unique_ptr<base::DictionaryValue> download(new base::DictionaryValue());
  download->SetStringPath("default_directory", "/examples/python/downloads");
  prefs.SetDictionary("download", std::move(download));
  caps.SetPath({"goog:chromeOptions", "prefs"}, prefs.Clone());

  Status status = capabilities.Parse(caps);
  BrowserInfo binfo;
  binfo.is_headless = true;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  status = internal::ConfigureHeadlessSession(&session, capabilities);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(session.chrome->GetBrowserInfo()->is_headless);
  ASSERT_STREQ("/examples/python/downloads",
               session.headless_download_directory->c_str());
}

TEST(SessionCommandsTest, ConfigureHeadlessSession_noDownloadDir) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  base::Value::ListStorage args;
  args.emplace_back("headless");
  caps.SetPath({"goog:chromeOptions", "args"}, base::Value(args));

  Status status = capabilities.Parse(caps);
  BrowserInfo binfo;
  binfo.is_headless = true;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  status = internal::ConfigureHeadlessSession(&session, capabilities);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(session.chrome->GetBrowserInfo()->is_headless);
  ASSERT_STREQ(".", session.headless_download_directory->c_str());
}

TEST(SessionCommandsTest, ConfigureHeadlessSession_notHeadless) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  base::DictionaryValue prefs;
  std::unique_ptr<base::DictionaryValue> download(new base::DictionaryValue());
  download->SetStringPath("default_directory", "/examples/python/downloads");
  prefs.SetDictionary("download", std::move(download));
  caps.SetPath({"goog:chromeOptions", "prefs"}, prefs.Clone());

  Status status = capabilities.Parse(caps);
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  status = internal::ConfigureHeadlessSession(&session, capabilities);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_FALSE(session.chrome->GetBrowserInfo()->is_headless);
  ASSERT_FALSE(session.headless_download_directory);
}

TEST(SessionCommandsTest, ConfigureSession_allSet) {
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  const base::DictionaryValue* params_in = nullptr;
  base::Value value = base::JSONReader::Read(
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
      })")
                          .value();
  ASSERT_TRUE(value.GetAsDictionary(&params_in));

  const base::DictionaryValue* desired_caps_out;
  base::DictionaryValue merged_out;
  Capabilities capabilities_out;
  Status status = internal::ConfigureSession(
      &session, *params_in, &desired_caps_out, &merged_out, &capabilities_out);
  ASSERT_EQ(kOk, status.code()) << status.message();
  // Verify out parameters have been set
  ASSERT_TRUE(desired_caps_out->is_dict());
  ASSERT_TRUE(merged_out.is_dict());
  ASSERT_TRUE(capabilities_out.logging_prefs["driver"]);
  // Verify session settings are correct
  ASSERT_EQ(kAccept, session.unhandled_prompt_behavior);
  ASSERT_EQ(base::TimeDelta::FromSeconds(57), session.implicit_wait);
  ASSERT_EQ(base::TimeDelta::FromSeconds(29), session.page_load_timeout);
  ASSERT_EQ(base::TimeDelta::FromSeconds(21), session.script_timeout);
  ASSERT_TRUE(session.strict_file_interactability);
  ASSERT_EQ(Log::Level::kDebug, session.driver_log.get()->min_level());
}

TEST(SessionCommandsTest, ConfigureSession_defaults) {
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  const base::DictionaryValue* params_in = nullptr;
  base::Value value = base::JSONReader::Read(
                          R"({
        "capabilities": {
          "alwaysMatch": { },
          "firstMatch": [ { } ]
        }
      })")
                          .value();
  ASSERT_TRUE(value.GetAsDictionary(&params_in));
  const base::DictionaryValue* desired_caps_out;
  base::DictionaryValue merged_out;
  Capabilities capabilities_out;

  Status status = internal::ConfigureSession(
      &session, *params_in, &desired_caps_out, &merged_out, &capabilities_out);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(desired_caps_out->is_dict());
  ASSERT_TRUE(merged_out.is_dict());
  // Testing specific values could be fragile, but want to verify they are set
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), session.implicit_wait);
  ASSERT_EQ(base::TimeDelta::FromSeconds(300), session.page_load_timeout);
  ASSERT_EQ(base::TimeDelta::FromSeconds(30), session.script_timeout);
  ASSERT_FALSE(session.strict_file_interactability);
  ASSERT_EQ(Log::Level::kWarning, session.driver_log.get()->min_level());
  // w3c values:
  ASSERT_EQ(kDismissAndNotify, session.unhandled_prompt_behavior);
}

TEST(SessionCommandsTest, ConfigureSession_legacyDefault) {
  BrowserInfo binfo;
  MockChrome* chrome = new MockChrome(binfo);
  Session session("id", std::unique_ptr<Chrome>(chrome));

  const base::DictionaryValue* params_in = nullptr;
  base::Value value = base::JSONReader::Read(
                          R"({
        "desiredCapabilities": {
          "browserName": "chrome",
          "goog:chromeOptions": {
             "w3c": false
          }
        }
      })")
                          .value();
  ASSERT_TRUE(value.GetAsDictionary(&params_in));
  const base::DictionaryValue* desired_caps_out;
  base::DictionaryValue merged_out;
  Capabilities capabilities_out;

  Status status = internal::ConfigureSession(
      &session, *params_in, &desired_caps_out, &merged_out, &capabilities_out);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_TRUE(desired_caps_out->is_dict());
  // legacy values:
  ASSERT_EQ(kIgnore, session.unhandled_prompt_behavior);
}
