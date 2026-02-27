// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_policy_activity_log_filter_delegate.h"

#include "base/values.h"
#include "extensions/common/dom_action_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class ChromePolicyActivityLogFilterDelegateTest : public testing::Test {
 public:
  ChromePolicyActivityLogFilterDelegateTest() = default;
  ~ChromePolicyActivityLogFilterDelegateTest() override = default;

 protected:
  void SetUp() override {
    filter_delegate_ =
        std::make_unique<ChromePolicyActivityLogFilterDelegate>();
  }

  void TearDown() override { filter_delegate_.reset(); }

  ChromePolicyActivityLogFilterDelegate* filter() {
    return filter_delegate_.get();
  }

 private:
  std::unique_ptr<ChromePolicyActivityLogFilterDelegate> filter_delegate_;
};

TEST_F(ChromePolicyActivityLogFilterDelegateTest, ScriptAndIframeInjection) {
  const GURL kUrl("https://www.google.com");

  // 1. Add script tag.
  // Format: [arg0=tag, arg1=src]
  base::ListValue script_args;
  script_args.Append("script");
  script_args.Append("https://evil.com/evil.js");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                        "blinkAddElement", script_args, kUrl));

  // 2. Set script src.
  // Format: [arg0=tag, arg1=attr_name, arg2=old_val, arg3=new_val]
  base::ListValue script_src_args;
  script_src_args.Append("script");
  script_src_args.Append("src");
  script_src_args.Append("");
  script_src_args.Append("https://evil.com/evil.js");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                        "blinkSetAttribute", script_src_args,
                                        kUrl));

  // 3. Add iframe tag.
  // Format: [arg0=tag, arg1=src]
  base::ListValue iframe_args;
  iframe_args.Append("iframe");
  iframe_args.Append("https://evil.com/frame.html");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                        "blinkAddElement", iframe_args, kUrl));
}

TEST_F(ChromePolicyActivityLogFilterDelegateTest, FormHijacking) {
  const GURL kUrl("https://www.google.com");

  // 1. Add form tag.
  // Format: [arg0=tag, arg1=method, arg2=action]
  base::ListValue form_args;
  form_args.Append("form");
  form_args.Append("POST");
  form_args.Append("https://phish.com/login");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                        "blinkAddElement", form_args, kUrl));

  // 2. Set action on form.
  // Format: [arg0=tag, arg1=attr_name, arg2=old_val, arg3=new_val]
  base::ListValue form_action_args;
  form_action_args.Append("form");
  form_action_args.Append("action");
  form_action_args.Append("");
  form_action_args.Append("https://phish.com/login");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                        "blinkSetAttribute", form_action_args,
                                        kUrl));

  // 3. Set formaction on button.
  // Format: [arg0=tag, arg1=attr_name, arg2=old_val, arg3=new_val]
  base::ListValue button_args;
  button_args.Append("button");
  button_args.Append("formaction");
  button_args.Append("");
  button_args.Append("https://phish.com/login");
  EXPECT_TRUE(filter()->IsHighRiskEvent(
      "ext", DomActionType::METHOD, "blinkSetAttribute", button_args, kUrl));
}

TEST_F(ChromePolicyActivityLogFilterDelegateTest, ProtocolHandlers) {
  const GURL kUrl("https://www.google.com");

  // 1. a.href = javascript:...
  // Format: [arg0=tag, arg1=attr_name, arg2=old_val, arg3=new_val]
  base::ListValue link_args;
  link_args.Append("a");
  link_args.Append("href");
  link_args.Append("");
  link_args.Append("javascript:alert(1)");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                        "blinkSetAttribute", link_args, kUrl));

  // 2. link creation with data URL.
  // Format: [arg0=tag, arg1=rel, arg2=href]
  base::ListValue data_args;
  data_args.Append("link");
  data_args.Append("stylesheet");
  data_args.Append("data:text/css,body{color:red}");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                        "blinkAddElement", data_args, kUrl));
}

TEST_F(ChromePolicyActivityLogFilterDelegateTest, Confidentiality) {
  const GURL kUrl("https://www.google.com");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::GETTER,
                                        "Document.cookie", base::ListValue(),
                                        kUrl));
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::GETTER,
                                        "HTMLInputElement.value",
                                        base::ListValue(), kUrl));
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::GETTER,
                                        "HTMLTextAreaElement.value",
                                        base::ListValue(), kUrl));
}

TEST_F(ChromePolicyActivityLogFilterDelegateTest, BenignActivity) {
  const GURL kUrl("https://www.google.com");

  // 1. Benign tag addition.
  base::ListValue div_args;
  div_args.Append("div");
  EXPECT_FALSE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                         "blinkAddElement", div_args, kUrl));

  // 2. Benign attribute change.
  // Format: [arg0=tag, arg1=attr_name, arg2=old_val, arg3=new_val]
  base::ListValue script_id_args;
  script_id_args.Append("script");
  script_id_args.Append("id");
  script_id_args.Append("");
  script_id_args.Append("my-script");
  EXPECT_FALSE(filter()->IsHighRiskEvent(
      "ext", DomActionType::METHOD, "blinkSetAttribute", script_id_args, kUrl));
}

}  // namespace extensions
