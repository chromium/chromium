// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {

// Tests CommerceHintAgent in a renderer.
class CommerceHintAgentRendererTest : public ChromeRenderViewTest {
 public:
  CommerceHintAgentRendererTest() = default;

 protected:
  void PopulateForm(const std::string& innerHTML) {
    const char script_template[] =
        "document.getElementById('form').innerHTML = $1;";
    std::string script = content::JsReplace(script_template, innerHTML);
    ExecuteJavaScriptForTests(script.c_str());
  }
};

static const std::map<std::string, std::vector<std::string>> extract_case = {
    {"<button>Pay now</button>", {"Pay now"}},
    {"<button> Pay now  </button>", {"Pay now"}},
    {"<button><br><p><span><br>\nPay now</span>\n</p><br>\n</button>",
     {"Pay now"}},
    {"<p>hello</p><button>Pay now</button>", {"Pay now"}},
    // input[type="submit"]
    {"<input type=\"text\" value=\"value of input\">", {}},
    {"<input type=\"submit\" value=\"value of input\">", {"value of input"}},
    {"<input type=\"submit\" aria-labelledby=\"desc\" value=\"value of input\">"
     "<span id=\"desc\">label</span>",
     {"label", "value of input"}},
    {"<input type=\"submit\" aria-labelledby=\"nonexisting\">", {}},
    // Multiple buttons.
    {"<button>Pay now</button>something "
     "in<p>between</p><button>cancel</button>"
     "<input type=\"submit\" value=\"Submit\">",
     {"Pay now", "cancel", "Submit"}},
    {"<p>test</p>", {}},
    {"", {}},
    // Unicode.
    {"<button>&#x7d50;&#x8cec;</button>", {"結賬"}},
};

TEST_F(CommerceHintAgentRendererTest, ExtractButtonTexts) {
  LoadHTML("<form id='form'></form>");
  blink::WebFormElement form = GetMainFrame()
                                   ->GetDocument()
                                   .GetElementById(blink::WebString("form"))
                                   .ToConst<blink::WebFormElement>();

  for (auto& entry : extract_case) {
    PopulateForm(entry.first);
    const std::vector<std::string> button_texts =
        cart::CommerceHintAgent::ExtractButtonTexts(form);
    auto expected = entry.second;
    EXPECT_EQ(button_texts, expected) << "HTML = " << entry.first;
  }
}

}  // namespace
