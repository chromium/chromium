// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/controlled_frame/web_url_pattern_natives.h"

#include <optional>
#include <vector>

#include "base/containers/to_value_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/string_source_map.h"
#include "extensions/renderer/test_v8_extension_configuration.h"
#include "v8/include/v8.h"

namespace controlled_frame {

namespace {
v8::Local<v8::String> ToV8String(const char* str) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), str)
      .ToLocalChecked();
}
}  // namespace

class WebUrlPatternNativesBrowserTest : public ChromeRenderViewTest {
 public:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    v8::HandleScope handle_scope(Isolate());
    v8::Local<v8::Context> context = v8::Context::New(
        Isolate(),
        extensions::TestV8ExtensionConfiguration::GetConfiguration());
    v8::Context::Scope context_scope(context);
    script_context_ = std::make_unique<extensions::ScriptContext>(
        context, /* frame=*/nullptr,
        extensions::mojom::HostID(
            extensions::mojom::HostID::HostType::kControlledFrameEmbedder,
            "isolated-app://some_app_id"),
        /* extension=*/nullptr,
        /* blink_isolated_world_id=*/std::nullopt,
        extensions::mojom::ContextType::kWebPage,
        /* effective_extension=*/nullptr,
        extensions::mojom::ContextType::kWebPage);
    script_context_->SetModuleSystem(std::make_unique<extensions::ModuleSystem>(
        script_context_.get(), &source_map_));

    script_context_->module_system()->RegisterNativeHandler(
        "WebUrlPatternNatives",
        std::make_unique<controlled_frame::WebUrlPatternNatives>(
            script_context_.get()));
  }

  void TearDown() override {
    script_context_->Invalidate();
    ChromeRenderViewTest::TearDown();
  }

 protected:
  void RegisterAndRequireTestModule(const std::string& module_name,
                                    const std::string& module_source) {
    extensions::ModuleSystem::NativesEnabledScope enable_natives(
        script_context_->module_system());
    source_map_.RegisterModule(module_name, module_source);
    ASSERT_FALSE(script_context_->module_system()
                     ->Require(module_name)
                     .ToLocalChecked()
                     .IsEmpty());
  }

  std::optional<base::Value> CallModuleMethod(
      const std::string& module_name,
      const std::string& method_name,
      std::vector<v8::Local<v8::Value>>* args) {
    base::test::TestFuture<std::optional<base::Value>, base::TimeTicks> future;
    script_context_->module_system()->CallModuleMethodSafe(
        module_name, method_name, args->size(), args->data(),
        future.GetCallback());
    return std::get<0>(future.Take());
  }

 private:
  std::unique_ptr<extensions::ScriptContext> script_context_;
  extensions::StringSourceMap source_map_;
};

TEST_F(WebUrlPatternNativesBrowserTest, URLPatternToMatchPatterns) {
  v8::HandleScope handle_scope(Isolate());
  RegisterAndRequireTestModule("TestRunner", R"(
    const natives = requireNative('WebUrlPatternNatives');

    exports.runTest = function(urlPatternString) {
      try {
        return natives.URLPatternToMatchPatterns(urlPatternString);
      } catch(e) {
        return [];
      }
    })");

  struct URLPatternTestCase {
    std::string input_url_pattern;
    std::vector<std::string> expected_match_patterns;
  };

  const URLPatternTestCase kTestCases[] = {
      {"http://*/*", {"http://*/*", "http://*/*?*"}},
      {"*://*/*",
       {"*://*/*", "*://*/*?*", "ws://*/*", "wss://*/*", "ws://*/*?*",
        "wss://*/*?*"}},
      {"*://asdf.com/*",
       {"*://asdf.com/*", "*://asdf.com/*?*", "ws://asdf.com/*",
        "wss://asdf.com/*", "ws://asdf.com/*?*", "wss://asdf.com/*?*"}},
      {"https://*.asdf.com/fdsa",
       {"https://*.asdf.com/fdsa", "https://*.asdf.com/fdsa?*"}},
      {"ws://*.asdf.com/*", {"ws://*.asdf.com/*", "ws://*.asdf.com/*?*"}},
      {"wss://*/asdf/*/fdsa", {"wss://*/asdf/*/fdsa", "wss://*/asdf/*/fdsa?*"}},
      {"https://*", {"https://*/*", "https://*/*?*"}},
      {"https://*/", {"https://*/", "https://*/?*"}},
      {"https://*/asdf", {"https://*/asdf", "https://*/asdf?*"}},
      {"http://asdf.com", {"http://asdf.com/*", "http://asdf.com/*?*"}},
      {"http://asdf.com/fdsa",
       {"http://asdf.com/fdsa", "http://asdf.com/fdsa?*"}},
      {"http://asdf.com/fdsa/*",
       {"http://asdf.com/fdsa/*", "http://asdf.com/fdsa/*?*"}},
      {"http://asdf.com*", {}},  // only * or *.something in the domain
      {"ftp://asdf.com/", {}},   // no ftp
      {"asdf://asdf.com/", {}},  // wrong protocol
      {"http://*.asdf.com/", {"http://*.asdf.com/", "http://*.asdf.com/?*"}},
      {"http://*asdf.com/", {}},   // only * or *.something in the domain
      {"http://asdf.*.com/", {}},  // only * or *.something in the domain
      {"http:/asdf.com", {}},      // malformed protocol part
      {"http://asdf.com:8080/fdsa",
       {"http://asdf.com:8080/fdsa", "http://asdf.com:8080/fdsa?*"}},
      {"*://asdf.com:8080/fdsa", {}},
      {"http://asdf.com:80*/fdsa", {}},
      {"http://asdf.com:*80/fdsa", {}},
      {"*://asdf.com:*/fdsa",
       {"*://asdf.com:*/fdsa", "*://asdf.com:*/fdsa?*", "ws://asdf.com:*/fdsa",
        "wss://asdf.com:*/fdsa", "ws://asdf.com:*/fdsa?*",
        "wss://asdf.com:*/fdsa?*"}},
      {"http://asdf.com:*/fdsa",
       {"http://asdf.com:*/fdsa", "http://asdf.com:*/fdsa?*"}},
      {"http://asdf.com:fdsa/asdf", {}},
      {"http://asdf.com/fdsa?query", {"http://asdf.com/fdsa?query"}},
      {"http://asdf.com/?query=fdsa&query2=asdf",
       {"http://asdf.com/?query=fdsa&query2=asdf"}},
      {"http://asdf.com?query", {"http://asdf.com/?query"}},
      {"http://user@asdf.com/fdsa", {}},
      {"http://user:pass@asdf.com/fdsa", {}},
      {"http://asdf.com/:group", {}},
      {"http://:asdf.com/", {}},
  };

  for (const auto& test : kTestCases) {
    std::vector<v8::Local<v8::Value>> args = {
        ToV8String(test.input_url_pattern.c_str())};
    std::optional<base::Value> match_patterns =
        CallModuleMethod("TestRunner", "runTest", &args);

    ASSERT_TRUE(match_patterns.has_value());
    EXPECT_EQ(match_patterns.value(),
              base::ToValueList(test.expected_match_patterns));
  }
}

}  // namespace controlled_frame
