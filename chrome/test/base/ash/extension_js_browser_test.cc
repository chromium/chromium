// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/extension_js_browser_test.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ash/javascript_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/service_worker_host.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension.h"
#include "ui/base/ime/ash/extension_ime_util.h"

namespace {
using extensions::service_worker_test_utils::TestServiceWorkerTaskQueueObserver;

// Class to observe service worker readiness for the execution of test JS.
class ExtensionTestObserver : public extensions::ExtensionRegistryObserver {
 public:
  explicit ExtensionTestObserver(const char* extension_id,
                                 content::BrowserContext* context)
      : extension_id_(extension_id), context_(context) {
    extensions::ExtensionRegistry::Get(context_)->AddObserver(this);
  }

  ~ExtensionTestObserver() override {
    extensions::ExtensionRegistry::Get(context_)->RemoveObserver(this);
  }

  int WaitForManifestVersion() {
    if (manifest_version_) {
      return manifest_version_;
    }
    base::RunLoop waiter;
    manifest_quit_ = waiter.QuitClosure();
    waiter.Run();
    return manifest_version_;
  }

  void WaitForServiceWorkerStart() {
    started_observer.WaitForWorkerStarted(extension_id_);
  }

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* context,
                         const extensions::Extension* extension) override {
    if (context == context_ && extension->id() == extension_id_) {
      manifest_version_ = extension->manifest_version();
      if (manifest_quit_) {
        std::move(manifest_quit_).Run();
      }
    }
  }

 private:
  const std::string extension_id_;
  // Not owned.
  raw_ptr<content::BrowserContext> context_;
  size_t manifest_version_ = 0;
  base::OnceClosure manifest_quit_;
  TestServiceWorkerTaskQueueObserver started_observer;
};

const std::vector<std::string>& GetExtensionIdsToCollectCoverage() {
  static const std::vector<std::string> extensions_for_coverage = {
      extension_misc::kChromeVoxExtensionId,
      extension_misc::kSelectToSpeakExtensionId,
      extension_misc::kSwitchAccessExtensionId,
      extension_misc::kAccessibilityCommonExtensionId,
      extension_misc::kEnhancedNetworkTtsExtensionId,
      ash::extension_ime_util::kBrailleImeExtensionId,
  };
  return extensions_for_coverage;
}

}  // namespace

ExtensionJSBrowserTest::ExtensionJSBrowserTest() = default;

ExtensionJSBrowserTest::~ExtensionJSBrowserTest() = default;

void ExtensionJSBrowserTest::SetUpOnMainThread() {
  JavaScriptBrowserTest::SetUpOnMainThread();

  // Set up coverage collection.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevtoolsCodeCoverage)) {
    base::FilePath devtools_code_coverage_dir =
        command_line->GetSwitchValuePath(switches::kDevtoolsCodeCoverage);
    ShouldInspectDevToolsAgentHostCallback callback =
        base::BindRepeating([](content::DevToolsAgentHost* host) {
          const auto& ext_ids = GetExtensionIdsToCollectCoverage();
          for (const auto& ext_id : ext_ids) {
            if (base::Contains(host->GetURL().path(), ext_id) &&
                host->GetType() == "background_page") {
              return true;
            }
          }
          return false;
        });
    coverage_handler_ = std::make_unique<DevToolsAgentCoverageObserver>(
        devtools_code_coverage_dir, std::move(callback));
  }
}

void ExtensionJSBrowserTest::WaitForExtension(const char* extension_id,
                                              base::OnceClosure load_cb) {
  extension_id_ = extension_id;

  // ExtensionHosts only exist when there is an associated RenderFrame.
  // Initialize both an ExtensionHostTestHelper and a ServiceWorkerObserver
  // before running the load callback, to avoid missing the relevant event.
  extensions::ExtensionHostTestHelper host_helper(GetProfile(), extension_id);
  ExtensionTestObserver observer(extension_id, GetProfile());
  std::move(load_cb).Run();

  if (observer.WaitForManifestVersion() == 3) {
    observer.WaitForServiceWorkerStart();
    extension_host_browser_context_ = GetProfile();
    return;
  }

  extensions::ExtensionHost* extension_host =
      host_helper.WaitForHostCompletedFirstLoad();
  ASSERT_TRUE(extension_host);
  extension_host_browser_context_ = extension_host->browser_context();
}

bool ExtensionJSBrowserTest::RunJavascriptTestF(bool is_async,
                                                const std::string& test_fixture,
                                                const std::string& test_name) {
  if (!extension_host_browser_context_) {
    ADD_FAILURE() << "ExtensionHost failed to start";
    return false;
  }
  std::vector<std::u16string> scripts;

  base::Value::Dict test_runner_params;
  if (embedded_test_server()->Started()) {
    test_runner_params.Set("testServerBaseUrl",
                           embedded_test_server()->base_url().spec());
  }

  if (!libs_loaded_) {
    BuildJavascriptLibraries(&scripts);
    libs_loaded_ = true;
  }

  scripts.push_back(base::UTF8ToUTF16(content::JsReplace(
      "const testRunnerParams = $1;", std::move(test_runner_params))));

  scripts.push_back(BuildRunTestJSCall(
      is_async, "RUN_TEST_F",
      base::Value::List().Append(test_fixture).Append(test_name)));

  std::u16string script_16 = base::JoinString(scripts, u"\n");
  std::string script = base::UTF16ToUTF8(script_16);

  auto result = extensions::BackgroundScriptExecutor::ExecuteScript(
      extension_host_browser_context_, extension_id_, script,
      extensions::BackgroundScriptExecutor::ResultCapture::kSendScriptResult);

  if (!result.is_string())
    return false;

  if (coverage_handler_ && coverage_handler_->CoverageEnabled()) {
    const std::string& full_test_name = base::StrCat({test_fixture, test_name});
    coverage_handler_->CollectCoverage(full_test_name);
  }

  std::string result_str = result.GetString();
  std::optional<base::Value> value_result = base::JSONReader::Read(result_str);
  const base::Value::Dict& dict_value = value_result->GetDict();

  bool test_result = dict_value.FindBool("result").value();
  const std::string* test_result_message = dict_value.FindString("message");
  CHECK(test_result_message);
  if (!test_result_message->empty()) {
    ADD_FAILURE() << *test_result_message;
  }
  return test_result;
}
