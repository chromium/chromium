// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui_browsertest.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class NetInternalsBrowserTest : public NetInternalsTest {
 protected:
  NetInternalsBrowserTest() {
    set_test_loader_host(chrome::kChromeUINetInternalsHost);
  }
};

class NetInternalsDnsViewTest : public NetInternalsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    RunTestWithoutTestLoader(
        "net_internals/dns_view_test.js",
        base::StringPrintf("runMochaTest('NetInternalsDnsViewTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(NetInternalsDnsViewTest, ResolveHostWithoutAlternative) {
  RunTestCase("ResolveHostWithoutAlternative");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDnsViewTest, ResolveHostWithECHAlternative) {
  RunTestCase("ResolveHostWithECHAlternative");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDnsViewTest,
                       ResolveHostWithMultipleAlternatives) {
  RunTestCase("ResolveHostWithMultipleAlternatives");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDnsViewTest, ErrorNameNotResolved) {
  RunTestCase("ErrorNameNotResolved");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDnsViewTest, ClearCache) {
  RunTestCase("ClearCache");
}

using NetInternalsMainTest = NetInternalsBrowserTest;

IN_PROC_BROWSER_TEST_F(NetInternalsMainTest, TabVisibility) {
  RunTestWithoutTestLoader("net_internals/main_test.js", "mocha.run()");
}

class NetInternalsDomainSecurityPolicyViewTest
    : public NetInternalsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    RunTestWithoutTestLoader(
        "net_internals/domain_security_policy_view_test.js",
        base::StringPrintf(
            "runMochaTest('DomainSecurityPolicyViewTest', '%s');",
            testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest,
                       QueryNotFound) {
  RunTestCase("QueryNotFound");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest, QueryError) {
  RunTestCase("QueryError");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest,
                       DeleteNotFound) {
  RunTestCase("DeleteNotFound");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest, DeleteError) {
  RunTestCase("DeleteError");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest, AddDelete) {
  RunTestCase("AddDelete");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest, AddFail) {
  RunTestCase("AddFail");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest, AddError) {
  RunTestCase("AddError");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest, AddOverwrite) {
  RunTestCase("AddOverwrite");
}

IN_PROC_BROWSER_TEST_F(NetInternalsDomainSecurityPolicyViewTest, AddTwice) {
  RunTestCase("AddTwice");
}

class NetInternalsSharedDictionaryViewTest : public NetInternalsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    RunTestWithoutTestLoader(
        "net_internals/shared_dictionary_view_test.js",
        base::StringPrintf(
            "runMochaTest('NetInternalsSharedDictionaryViewTest', '%s');",
            testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(NetInternalsSharedDictionaryViewTest, ReloadEmpty) {
  RunTestCase("ReloadEmpty");
}

IN_PROC_BROWSER_TEST_F(NetInternalsSharedDictionaryViewTest,
                       ReloadOneDictionary) {
  RunTestCase("ReloadOneDictionary");
}

IN_PROC_BROWSER_TEST_F(NetInternalsSharedDictionaryViewTest,
                       ReloadTwoDictionaries) {
  RunTestCase("ReloadTwoDictionaries");
}

IN_PROC_BROWSER_TEST_F(NetInternalsSharedDictionaryViewTest,
                       ReloadTwoIsolations) {
  RunTestCase("ReloadTwoIsolations");
}

IN_PROC_BROWSER_TEST_F(NetInternalsSharedDictionaryViewTest,
                       ClearForIsolation) {
  RunTestCase("ClearForIsolation");
}

IN_PROC_BROWSER_TEST_F(NetInternalsSharedDictionaryViewTest, ClearAll) {
  RunTestCase("ClearAll");
}
