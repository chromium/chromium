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

constexpr struct extract_case {
  const char* html;
  const char* answer;
} extract_cases[] = {
    {
        "<button>Pay now</button>",
        "Pay now",
    },
    {
        "<button> Pay now  </button>",
        "Pay now",
    },
    {
        "<button> <br>  Pay&nbsp;now <br>  </button>",
        "Pay now",
    },
    {
        "<button><br><p><span><br>\nPay now</span>\n</p><br>\n</button>",
        "Pay now",
    },
    {
        "<p>hello</p><button>Pay now</button>",
        "Pay now",
    },
    {
        // Multiple buttons.
        "<button>Pay now</button>something in "
        "<p>between</p><button>cancel</button>",
        "Pay now cancel",
    },
    {
        "<p>test</p>",
        "",
    },
    {
        "",
        "",
    },
    {
        // Unicode.
        "<button>&#x7d50;&#x8cec;</button>",
        "結賬",
    },
};

TEST_F(CommerceHintAgentRendererTest, ExtractButtonText) {
  LoadHTML("<form id='form'></form>");
  blink::WebFormElement form = GetMainFrame()
                                   ->GetDocument()
                                   .GetElementById(blink::WebString("form"))
                                   .ToConst<blink::WebFormElement>();

  for (auto& value : extract_cases) {
    PopulateForm(value.html);
    EXPECT_EQ(value.answer, cart::CommerceHintAgent::ExtractButtonText(form))
        << "HTML = " << value.html;
  }
}

}  // namespace
