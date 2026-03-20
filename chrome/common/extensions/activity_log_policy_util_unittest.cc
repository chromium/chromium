// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/activity_log_policy_util.h"

#include "base/values.h"
#include "extensions/common/dom_action_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::activity_log_policy_util {

using SignalType = TelemetrySignalType;

TEST(ActivityLogPolicyUtilNamespaceTest, IsHighRiskEvent) {
  EXPECT_FALSE(IsHighRiskEvent(SignalType::kNone));
  EXPECT_TRUE(IsHighRiskEvent(SignalType::kDOMAccess));
  EXPECT_TRUE(IsHighRiskEvent(SignalType::kScriptInjection));
}

TEST(ActivityLogPolicyUtilNamespaceTest,
     GetTelemetrySignalType_Confidentiality) {
  base::ListValue empty_args;

  // 1. GETTER should be allowed.
  EXPECT_EQ(SignalType::kDOMAccess,
            GetTelemetrySignalType("Document.cookie", empty_args,
                                   DomActionType::GETTER));
  EXPECT_EQ(SignalType::kDOMAccess,
            GetTelemetrySignalType("HTMLInputElement.value", empty_args,
                                   DomActionType::GETTER));
  EXPECT_EQ(SignalType::kDOMAccess,
            GetTelemetrySignalType("HTMLTextAreaElement.value", empty_args,
                                   DomActionType::GETTER));

  // 2. SETTER should be filtered.
  EXPECT_EQ(SignalType::kNone,
            GetTelemetrySignalType("Document.cookie", empty_args,
                                   DomActionType::SETTER));

  // 3. Catch-all (MODIFIED) should be allowed (for Browser-side logic).
  EXPECT_EQ(SignalType::kDOMAccess,
            GetTelemetrySignalType("Document.cookie", empty_args,
                                   DomActionType::MODIFIED));
}

TEST(ActivityLogPolicyUtilNamespaceTest,
     GetTelemetrySignalType_DirectScriptExecution) {
  base::ListValue empty_args;

  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("scripting.executeScript", empty_args,
                                   DomActionType::METHOD));
}

TEST(ActivityLogPolicyUtilNamespaceTest,
     GetTelemetrySignalType_ScriptAndIframeInjection) {
  // 1. Add script tag. [arg0=tag, arg1=src]
  base::ListValue script_args;
  script_args.Append("script");
  script_args.Append("https://evil.com/evil.js");
  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("blinkAddElement", script_args,
                                   DomActionType::METHOD));

  // 2. Set script src. [arg0=tag, arg1=attr_name, arg2=old_val, arg3=new_val]
  base::ListValue script_src_args;
  script_src_args.Append("script");
  script_src_args.Append("src");
  script_src_args.Append("");
  script_src_args.Append("https://evil.com/evil.js");
  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("blinkSetAttribute", script_src_args,
                                   DomActionType::METHOD));

  // 3. Add iframe tag. [arg0=tag, arg1=src]
  base::ListValue iframe_args;
  iframe_args.Append("iframe");
  iframe_args.Append("https://evil.com/frame.html");
  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("blinkAddElement", iframe_args,
                                   DomActionType::METHOD));
}

TEST(ActivityLogPolicyUtilNamespaceTest, GetTelemetrySignalType_FormHijacking) {
  // 1. Add form tag. [arg0=tag, arg1=method, arg2=action]
  base::ListValue form_args;
  form_args.Append("form");
  form_args.Append("POST");
  form_args.Append("https://phish.com/login");
  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("blinkAddElement", form_args,
                                   DomActionType::METHOD));

  // 2. Set action on form. [arg0=tag, arg1=attr_name, arg2=old_val,
  // arg3=new_val]
  base::ListValue form_action_args;
  form_action_args.Append("form");
  form_action_args.Append("action");
  form_action_args.Append("");
  form_action_args.Append("https://phish.com/login");
  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("blinkSetAttribute", form_action_args,
                                   DomActionType::METHOD));

  // 3. Set formaction on button. [arg0=tag, arg1=type, arg2=formmethod,
  // arg3=formaction]
  base::ListValue button_args;
  button_args.Append("button");
  button_args.Append("submit");
  button_args.Append("");  // empty formmethod
  button_args.Append("https://phish.com/login");
  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("blinkAddElement", button_args,
                                   DomActionType::METHOD));
}

TEST(ActivityLogPolicyUtilNamespaceTest,
     GetTelemetrySignalType_ProtocolHandlers) {
  // 1. a.href = javascript:... [arg0=tag, arg1=attr_name, arg2=old_val,
  // arg3=new_val]
  base::ListValue link_args;
  link_args.Append("a");
  link_args.Append("href");
  link_args.Append("");
  link_args.Append("javascript:alert(1)");
  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("blinkSetAttribute", link_args,
                                   DomActionType::METHOD));

  // 2. link creation with data URL. [arg0=tag, arg1=rel, arg2=href]
  base::ListValue data_args;
  data_args.Append("link");
  data_args.Append("stylesheet");
  data_args.Append("data:text/css,body{color:red}");
  EXPECT_EQ(SignalType::kScriptInjection,
            GetTelemetrySignalType("blinkAddElement", data_args,
                                   DomActionType::METHOD));
}

TEST(ActivityLogPolicyUtilNamespaceTest,
     GetTelemetrySignalType_BenignActivity) {
  // 1. Benign tag addition.
  base::ListValue div_args;
  div_args.Append("div");
  EXPECT_EQ(SignalType::kNone,
            GetTelemetrySignalType("blinkAddElement", div_args,
                                   DomActionType::METHOD));

  // 2. Benign attribute change. [arg0=tag, arg1=attr_name, arg2=old_val,
  // arg3=new_val]
  base::ListValue script_id_args;
  script_id_args.Append("script");
  script_id_args.Append("id");
  script_id_args.Append("");
  script_id_args.Append("my-script");
  EXPECT_EQ(SignalType::kNone,
            GetTelemetrySignalType("blinkSetAttribute", script_id_args,
                                   DomActionType::METHOD));

  // 3. Unrelated API call.
  EXPECT_EQ(SignalType::kNone,
            GetTelemetrySignalType("tabs.query", base::ListValue(),
                                   DomActionType::METHOD));
}

TEST(ActivityLogPolicyUtilNamespaceTest,
     GetArgumentsList_BlinkSetAttributeDropsOldValue) {
  base::ListValue args;
  args.Append("script");
  args.Append("src");
  args.Append("old_value.js");  // Should be dropped
  args.Append("new_value.js");

  std::vector<std::string> result = GetArgumentsList("blinkSetAttribute", args);

  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "script");
  EXPECT_EQ(result[1], "src");
  EXPECT_EQ(result[2], "new_value.js");
}

TEST(ActivityLogPolicyUtilNamespaceTest,
     GetArgumentsList_GenericKeepsAllNonEmptyStrings) {
  base::ListValue args;
  args.Append("arg1");
  args.Append("");   // Should be ignored (empty string)
  args.Append(123);  // Should be ignored (not a string)
  args.Append("arg2");

  std::vector<std::string> result =
      GetArgumentsList("scripting.executeScript", args);

  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "arg1");
  EXPECT_EQ(result[1], "arg2");
}

TEST(ActivityLogPolicyUtilNamespaceTest,
     GetArgumentsList_ScriptingExecuteScript) {
  // 1. Test "files" extraction.
  {
    base::ListValue args;
    base::DictValue dict;
    base::ListValue files;
    files.Append("script1.js");
    files.Append("script2.js");
    dict.Set("files", std::move(files));
    args.Append(std::move(dict));

    std::vector<std::string> result =
        GetArgumentsList("scripting.executeScript", args);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "script1.js");
    EXPECT_EQ(result[1], "script2.js");
  }

  // 2. Test "func" extraction.
  {
    base::ListValue args;
    base::DictValue dict;
    dict.Set("func", "function() { console.log('hello'); }");
    args.Append(std::move(dict));

    std::vector<std::string> result =
        GetArgumentsList("scripting.executeScript", args);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "function() { console.log('hello'); }");
  }

  // 3. Test "func" string capping at 1024 characters.
  {
    base::ListValue args;
    base::DictValue dict;
    std::string long_func(2000, 'A');
    dict.Set("func", long_func);
    args.Append(std::move(dict));

    std::vector<std::string> result =
        GetArgumentsList("scripting.executeScript", args);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].length(), 1024u);
    EXPECT_EQ(result[0], std::string(1024, 'A'));
  }
}

}  // namespace extensions::activity_log_policy_util
