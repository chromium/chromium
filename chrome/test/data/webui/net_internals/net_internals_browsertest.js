// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The way these tests work is as follows:
 * C++ in net_internals_ui_browsertest.cc does any necessary setup, and then
 * calls the entry point for a test with RunJavascriptTest.  The called
 * function can then use the assert/expect functions defined in test_api.js.
 * All callbacks from the browser are wrapped in such a way that they can
 * also use the assert/expect functions.
 *
 * A test ends when testDone is called.  This can be done by the test itself,
 * but will also be done by the test framework when an assert/expect test fails
 * or an exception is thrown.
 */
GEN_INCLUDE(
    ['//chrome/test/data/webui/net_internals/net_internals_test_base.js']);

// Include the C++ browser test class when generating *.cc files.
GEN('#include ' +
    '"chrome/browser/ui/webui/net_internals/net_internals_ui_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends NetInternalsTest
 */
function NetInternalsBrowserTest() {
  NetInternalsTest.call(this);
}

NetInternalsBrowserTest.prototype = {
  __proto__: NetInternalsTest.prototype,

  /** @inheritDoc */
  browsePreload: 'chrome://net-internals/',

  setUp: function() {
    NetInternalsTest.prototype.setUp.call(this);
    NetInternalsTest.activeTest = this;
  },

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

/**
 * @constructor
 * @extends NetInternalsBrowserTest
 */
function NetInternalsDnsViewTest() {}

NetInternalsDnsViewTest.prototype = {
  __proto__: NetInternalsBrowserTest.prototype,

  browsePreload:
      'chrome://net-internals/index.html?module=net_internals/dns_view_test.js',
};

TEST_F('NetInternalsDnsViewTest', 'ClearCache', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends NetInternalsBrowserTest
 */
function NetInternalsMainTest() {}

NetInternalsMainTest.prototype = {
  __proto__: NetInternalsBrowserTest.prototype,

  browsePreload:
      'chrome://net-internals/index.html?module=net_internals/main_test.js',
};

TEST_F('NetInternalsMainTest', 'TabVisibility', function() {
  mocha.run();
});

function NetInternalsDomainSecurityPolicyViewTest() {}
NetInternalsDomainSecurityPolicyViewTest.prototype = {
  __proto__: NetInternalsBrowserTest.prototype,

  browsePreload:
      'chrome://net-internals/index.html?module=net_internals/domain_security_policy_view_test.js',

  get suiteName() {
    return domain_security_policy_view_test.suiteName;
  },

  /** @param {string} testName The name of the test to run. */
  runMochaTest: function(testName) {
    runMochaTest(this.suiteName, testName);
  },
};

TEST_F('NetInternalsDomainSecurityPolicyViewTest', 'QueryNotFound', function() {
  this.runMochaTest(domain_security_policy_view_test.TestNames.QueryNotFound);
});

TEST_F('NetInternalsDomainSecurityPolicyViewTest', 'QueryError', function() {
  this.runMochaTest(domain_security_policy_view_test.TestNames.QueryError);
});

TEST_F(
    'NetInternalsDomainSecurityPolicyViewTest', 'DeleteNotFound', function() {
      this.runMochaTest(
          domain_security_policy_view_test.TestNames.DeleteNotFound);
    });

TEST_F('NetInternalsDomainSecurityPolicyViewTest', 'DeleteError', function() {
  this.runMochaTest(domain_security_policy_view_test.TestNames.DeleteError);
});

TEST_F('NetInternalsDomainSecurityPolicyViewTest', 'AddDelete', function() {
  this.runMochaTest(domain_security_policy_view_test.TestNames.AddDelete);
});

TEST_F('NetInternalsDomainSecurityPolicyViewTest', 'AddFail', function() {
  this.runMochaTest(domain_security_policy_view_test.TestNames.AddFail);
});

TEST_F('NetInternalsDomainSecurityPolicyViewTest', 'AddError', function() {
  this.runMochaTest(domain_security_policy_view_test.TestNames.AddError);
});

TEST_F('NetInternalsDomainSecurityPolicyViewTest', 'AddOverwrite', function() {
  this.runMochaTest(domain_security_policy_view_test.TestNames.AddOverwrite);
});

TEST_F('NetInternalsDomainSecurityPolicyViewTest', 'AddTwice', function() {
  this.runMochaTest(domain_security_policy_view_test.TestNames.AddTwice);
});

TEST_F(
    'NetInternalsDomainSecurityPolicyViewTest', 'ExpectCTQueryNotFound',
    function() {
      this.runMochaTest(
          domain_security_policy_view_test.TestNames.ExpectCTQueryNotFound);
    });

TEST_F(
    'NetInternalsDomainSecurityPolicyViewTest', 'ExpectCTQueryError',
    function() {
      this.runMochaTest(
          domain_security_policy_view_test.TestNames.ExpectCTQueryError);
    });

TEST_F(
    'NetInternalsDomainSecurityPolicyViewTest', 'ExpectCTAddDelete',
    function() {
      this.runMochaTest(
          domain_security_policy_view_test.TestNames.ExpectCTAddDelete);
    });

TEST_F(
    'NetInternalsDomainSecurityPolicyViewTest', 'ExpectCTAddFail', function() {
      this.runMochaTest(
          domain_security_policy_view_test.TestNames.ExpectCTAddFail);
    });

TEST_F(
    'NetInternalsDomainSecurityPolicyViewTest', 'ExpectCTAddOverwrite',
    function() {
      this.runMochaTest(
          domain_security_policy_view_test.TestNames.ExpectCTAddOverwrite);
    });

TEST_F(
    'NetInternalsDomainSecurityPolicyViewTest', 'ExpectCTAddTwice', function() {
      this.runMochaTest(
          domain_security_policy_view_test.TestNames.ExpectCTAddTwice);
    });

TEST_F(
    'NetInternalsDomainSecurityPolicyViewTest', 'ExpectCTTestReport',
    function() {
      this.runMochaTest(
          domain_security_policy_view_test.TestNames.ExpectCTTestReport);
    });
