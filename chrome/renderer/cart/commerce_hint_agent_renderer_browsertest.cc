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

TEST_F(CommerceHintAgentRendererTest, IsAddToCartButton) {
  const char* html = R"HTML(
    <style>
      .add-to-cart-button {
        height: 50px;
        width: 500px;
      }
    </style>
    <body>
      <!-- Correct Buttons -->
      <button class="add-to-cart-button correct-button"> Add to cart </button>

      <button class="add-to-cart-button">
        <div class="correct-button">  </div>
        Add to cart
      </button>

      <input class="correct-button" value="Add To Cart"></input>

      <!-- Wrong Buttons -->
      <button class="add-to-cart-button wrong-button"> Add to cat </button>

      <button class="add-to-cart-button wrong-button"> Add to   cart </button>

      <button style="height: 50px; width: 1000px" class="wrong-button">
        Add to cart
      </button>

      <div class="add-to-cart-button wrong-button"> Add to cart </div>

      <button class="add-to-cart-button">
        <button class="wrong-button"> Test </button>
        Add to cart
      </button>

      <button style="height: 200px; width: 500px">
        <button class="wrong-button"> </button>
        Add to cart
      </button>

    <button class="add-to-cart-button wrong-button">加入购物车</button>
    </body>
  )HTML";

  LoadHTML(html);

  auto correct_buttons = GetMainFrame()->GetDocument().QuerySelectorAll(
      blink::WebString(".correct-button"));
  EXPECT_GT(correct_buttons.size(), 0u);
  for (auto& element : correct_buttons) {
    EXPECT_TRUE(cart::CommerceHintAgent::IsAddToCartButton(element));
  }

  auto wrong_buttons = GetMainFrame()->GetDocument().QuerySelectorAll(
      blink::WebString(".wrong-button"));
  EXPECT_GT(wrong_buttons.size(), 0u);
  for (auto& element : wrong_buttons) {
    EXPECT_FALSE(cart::CommerceHintAgent::IsAddToCartButton(element));
  }

  blink::WebElement empty_element = blink::WebElement();
  EXPECT_TRUE(empty_element.IsNull());
  EXPECT_FALSE(cart::CommerceHintAgent::IsAddToCartButton(empty_element));
}

}  // namespace
