// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for generated tests.
 * @extends {testing.Test}
 */
function CertificateViewerUITest() {}

CertificateViewerUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  isAsync: true,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],

  /**
   * Define the C++ fixture class and include it.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'CertificateViewerUITest',

  /**
   * Show the certificate viewer dialog.
   */
  testGenPreamble: function() {
    GEN('ShowCertificateViewerGoogleCert();');
  },
};

var CertificateViewerUIInvalidCertTest = class extends CertificateViewerUITest {
  get testGenPreamble() {
    return () => {
      GEN('ShowCertificateViewerInvalidCert();');
    };
  }
};

// Helper for loading the Mocha test file as a JS module. Not using
// test_loader.html, as the test code needs to be loaded in the context of the
// dialog triggered with the ShowCertificateViewer() C++ call above.
function loadTestModule() {
  const scriptPolicy =
      window.trustedTypes.createPolicy('certificate-test-script', {
        createScriptURL: () =>
            'chrome://webui-test/certificate_viewer_dialog_test.js',
      });
  const s = document.createElement('script');
  s.type = 'module';
  s.src = scriptPolicy.createScriptURL('');
  document.body.appendChild(s);
  return new Promise(function(resolve, reject) {
    s.addEventListener('load', () => resolve());
  });
}

// Include the bulk of c++ code.
// Certificate viewer UI tests are disabled on platforms with native certificate
// viewers.
GEN('#include "chrome/test/data/webui/certificate_viewer_ui_test-inl.h"');
GEN('');

// Constructors and destructors must be provided in .cc to prevent clang errors.
GEN('CertificateViewerUITest::CertificateViewerUITest() {}');
GEN('CertificateViewerUITest::~CertificateViewerUITest() {}');

TEST_F('CertificateViewerUITest', 'DialogURL', function() {
  loadTestModule().then(() => {
    mocha.grep('DialogURL').run();
  });
});

TEST_F('CertificateViewerUITest', 'CommonName', function() {
  loadTestModule().then(() => {
    mocha.grep('CommonName').run();
  });
});

TEST_F('CertificateViewerUITest', 'Details', function() {
  loadTestModule().then(() => {
    mocha.grep('Details').run();
  });
});

TEST_F('CertificateViewerUIInvalidCertTest', 'InvalidCert', function() {
  loadTestModule().then(() => {
    mocha.grep('InvalidCert').run();
  });
});
