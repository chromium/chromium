// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The way these tests work is as follows:
 * C++ in net_internals_ui_browsertest.cc does any necessary setup, and then
 * calls the entry point for a test with RunJavascriptTest.  The called
 * function can then use the assert functions defined in test_api.js.
 * All callbacks from the browser are wrapped in such a way that they can
 * also use the assert functions.
 *
 * A test ends when testDone is called.  This can be done by the test itself,
 * but will also be done by the test framework when an assert test fails
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

  /** @param {string} testName The name of the test to run. */
  runMochaTest: function(testName) {
    runMochaTest(dns_view_test.suiteName, testName);
  },
};

TEST_F('NetInternalsDnsViewTest', 'ResolveHostWithoutAlternative', function() {
  this.runMochaTest(dns_view_test.TestNames.ResolveHostWithoutAlternative);
});

TEST_F(
    'NetInternalsDnsViewTest', 'ResolveHostWithHTTP2Alternative', function() {
      this.runMochaTest(
          dns_view_test.TestNames.ResolveHostWithHTTP2Alternative);
    });

TEST_F(
    'NetInternalsDnsViewTest', 'ResolveHostWithHTTP3Alternative', function() {
      this.runMochaTest(
          dns_view_test.TestNames.ResolveHostWithHTTP3Alternative);
    });

TEST_F('NetInternalsDnsViewTest', 'ResolveHostWithECHAlternative', function() {
  this.runMochaTest(dns_view_test.TestNames.ResolveHostWithECHAlternative);
});

TEST_F(
    'NetInternalsDnsViewTest', 'ResolveHostWithMultipleAlternatives',
    function() {
      this.runMochaTest(
          dns_view_test.TestNames.ResolveHostWithMultipleAlternatives);
    });

TEST_F('NetInternalsDnsViewTest', 'ErrorNameNotResolved', function() {
  this.runMochaTest(dns_view_test.TestNames.ErrorNameNotResolved);
});

TEST_F('NetInternalsDnsViewTest', 'ClearCache', function() {
  this.runMochaTest(dns_view_test.TestNames.ClearCache);
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
