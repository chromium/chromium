// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

/**
 * TestFixture for kiosk certification manager dialog WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function CertificateManagerDialogWebUITest() {}

CertificateManagerDialogWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the certification manager dialog page.
   */
  browsePreload: 'chrome://certificate-manager/',

  isAsync: true,
};

// crbug.com/682497
GEN('#if defined(ADDRESS_SANITIZER)');
GEN('#define MAYBE_Basic DISABLED_Basic');
GEN('#else');
GEN('#define MAYBE_Basic Basic');
GEN('#endif');
// Sanity test of the WebUI could be opened with no errors.
TEST_F('CertificateManagerDialogWebUITest', 'MAYBE_Basic', async function() {
  const {assertEquals} = await import('chrome://webui-test/chai_assert.js');
  assertEquals(this.browsePreload, document.location.href);
  testDone();
});
