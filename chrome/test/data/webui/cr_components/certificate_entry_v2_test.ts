// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import 'chrome://resources/cr_components/certificate_manager/certificate_entry_v2.js';
import 'chrome://certificate-manager/strings.m.js';

import type {CertificateEntryV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_entry_v2.js';
import {CertificateSource} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificates_v2_browser_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestCertificateManagerProxy} from './certificate_manager_v2_test_support.js';

suite('CertificateEntryV2Test', () => {
  let certEntry: CertificateEntryV2Element;
  let testProxy: TestCertificateManagerProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestCertificateManagerProxy();
    CertificatesV2BrowserProxy.setInstance(testProxy);
  });

  function initializeElement() {
    certEntry = document.createElement('certificate-entry-v2');
    certEntry.displayName = 'certname';
    certEntry.sha256hashHex = 'deadbeef';
    certEntry.certSource = CertificateSource.kChromeRootStore;
    document.body.appendChild(certEntry);
  }

  test('element check', async () => {
    initializeElement();

    assertEquals(
        'deadbeef', certEntry.$.certhash.value, 'wrong hash in input box');
    certEntry.$.view.click();
    const [source, hash] =
        await testProxy.handler.whenCalled('viewCertificate');
    assertEquals(
        CertificateSource.kChromeRootStore, source,
        'click provided wrong source');
    assertEquals('deadbeef', hash, 'click provided wrong hash');
  });
});
