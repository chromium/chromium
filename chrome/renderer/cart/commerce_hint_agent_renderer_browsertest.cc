// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"
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
    // Multiple buttons.
    {"<button>Pay now</button>something "
     "in<p>between</p><button>cancel</button>",
     {"Pay now", "cancel"}},
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
                                   .To<blink::WebFormElement>();

  for (auto& entry : extract_case) {
    PopulateForm(entry.first);
    const std::vector<std::string> button_texts =
        cart::CommerceHintAgent::ExtractButtonTexts(form);
    EXPECT_EQ(button_texts.size(), entry.second.size());
    for (size_t i = 0; i < button_texts.size(); i++) {
      EXPECT_EQ(button_texts.at(i), entry.second.at(i))
          << "HTML = " << entry.first;
    }
  }
}

}  // namespace
