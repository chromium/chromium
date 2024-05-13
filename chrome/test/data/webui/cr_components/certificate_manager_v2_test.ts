// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager v2 component.

import 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';

import type {CertificateEntryV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_entry_v2.js';
import type {CertificateManagerV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import type {CertificateManagerPageHandlerInterface, CertificateManagerPageRemote, SummaryCertInfo} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificateManagerPageCallbackRouter} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificates_v2_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';


class FakePageHandler extends TestBrowserProxy implements
    CertificateManagerPageHandlerInterface {
  private crsCerts_: SummaryCertInfo[] = [];
  private platformClientCerts_: SummaryCertInfo[] = [];
  private provisionedClientCerts_: SummaryCertInfo[] = [];

  constructor() {
    super([
      'getChromeRootStoreCerts',
      'getPlatformClientCerts',
      'getProvisionedClientCerts',
      'exportChromeRootStore',
      'viewCertificate',
    ]);
  }

  async getChromeRootStoreCerts(): Promise<{crsCertInfos: SummaryCertInfo[]}> {
    this.methodCalled('getChromeRootStoreCerts');
    return {'crsCertInfos': this.crsCerts_.slice()};
  }

  async getPlatformClientCerts(): Promise<{certs: SummaryCertInfo[]}> {
    this.methodCalled('getPlatformClientCerts');
    return {'certs': this.platformClientCerts_.slice()};
  }

  async getProvisionedClientCerts(): Promise<{certs: SummaryCertInfo[]}> {
    this.methodCalled('getProvisionedClientCerts');
    return {'certs': this.provisionedClientCerts_.slice()};
  }

  viewCertificate(sha256hashHex: string) {
    this.methodCalled('viewCertificate', sha256hashHex);
  }

  exportChromeRootStore() {
    this.methodCalled('exportChromeRootStore');
  }

  setChromeRootStoreCerts(crsCerts: SummaryCertInfo[]) {
    this.crsCerts_ = crsCerts;
  }

  setPlatformClientCerts(certs: SummaryCertInfo[]) {
    this.platformClientCerts_ = certs;
  }

  setProvisionedClientCerts(certs: SummaryCertInfo[]) {
    this.provisionedClientCerts_ = certs;
  }
}

class TestCertificateManagerProxy {
  callbackRouter: CertificateManagerPageCallbackRouter;
  callbackRouterRemote: CertificateManagerPageRemote;
  handler: FakePageHandler;

  constructor() {
    this.callbackRouter = new CertificateManagerPageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.handler = new FakePageHandler();
  }
}

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
    certEntry.set('displayName', 'certname');
    certEntry.set('sha256hashHex', 'deadbeef');
    document.body.appendChild(certEntry);
  }

  test('element check', async () => {
    initializeElement();

    assertEquals(
        'deadbeef', certEntry.$.certhash.value, 'wrong hash in input box');
    certEntry.$.view.click();
    const hash = await testProxy.handler.whenCalled('viewCertificate');
    assertEquals('deadbeef', hash, 'click provided wrong hash');
  });
});


suite('CertificateManagerV2Test', () => {
  let certManager: CertificateManagerV2Element;
  let testProxy: TestCertificateManagerProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestCertificateManagerProxy();
    CertificatesV2BrowserProxy.setInstance(testProxy);
  });

  function initializeElement() {
    certManager = document.createElement('certificate-manager-v2');
    document.body.appendChild(certManager);
  }

  test('Copy hash', async () => {
    const certs: SummaryCertInfo[] = [
      {
        'sha256hashHex': 'deadbeef',
        'displayName': 'cert1',
      },
    ];
    testProxy.handler.setChromeRootStoreCerts(certs);
    initializeElement();

    await testProxy.handler.whenCalled('getChromeRootStoreCerts');
    await microtasksFinished();
    assertFalse(certManager.$.toast.open);

    const certEntries =
        certManager.$.crsCerts.querySelectorAll('certificate-entry-v2');
    assertEquals(1, certEntries.length, 'no certs found');
    certEntries[0]!.$.copy.click();
    assertTrue(certManager.$.toast.open);
  });

  test('CRS list populated', async () => {
    const certs: SummaryCertInfo[] = [
      {
        'sha256hashHex': 'deadbeef',
        'displayName': 'cert1',
      },
    ];
    testProxy.handler.setChromeRootStoreCerts(certs);
    initializeElement();

    await testProxy.handler.whenCalled('getChromeRootStoreCerts');
    await microtasksFinished();

    const matchEls =
        certManager.$.crsCerts.querySelectorAll('certificate-entry-v2');
    assertEquals(1, matchEls.length, 'no certs displayed');
    assertEquals('cert1', matchEls[0]!.displayName);
    assertEquals('deadbeef', matchEls[0]!.sha256hashHex);
  });

  test('Export CRS certs', async () => {
    const certs: SummaryCertInfo[] = [
      {
        'sha256hashHex': 'deadbeef',
        'displayName': 'cert1',
      },
    ];
    testProxy.handler.setChromeRootStoreCerts(certs);
    initializeElement();

    await testProxy.handler.whenCalled('getChromeRootStoreCerts');
    await microtasksFinished();
    assertFalse(certManager.$.toast.open);

    const certEntries =
        certManager.$.crsCerts.querySelectorAll('certificate-entry-v2');
    assertEquals(1, certEntries.length, 'no certs found');
    certManager.$.exportCRS.click();
    await testProxy.handler.whenCalled('exportChromeRootStore');
  });

  test('platform client certs populated', async () => {
    const certs: SummaryCertInfo[] = [
      {
        'sha256hashHex': 'deadbeef',
        'displayName': 'cert1',
      },
    ];
    testProxy.handler.setPlatformClientCerts(certs);
    initializeElement();

    await testProxy.handler.whenCalled('getPlatformClientCerts');
    await microtasksFinished();

    const parent_element =
        certManager.shadowRoot!.querySelector('#platform-client-certs');
    assertTrue(!!parent_element, 'parent element not found');
    const matchEls = parent_element.querySelectorAll('.cert-row');
    assertEquals(1, matchEls.length, 'no certs displayed');
    // TODO(crbug.com/40928765): test the displayed name/hash
  });

  test('provisioned client certs populated', async () => {
    // <if expr="is_win or is_macosx">
    const certs: SummaryCertInfo[] = [
      {
        'sha256hashHex': 'deadbeef',
        'displayName': 'cert1',
      },
    ];
    testProxy.handler.setProvisionedClientCerts(certs);
    // </if>

    initializeElement();
    // <if expr="is_win or is_macosx">
    await testProxy.handler.whenCalled('getProvisionedClientCerts');
    // </if>
    await microtasksFinished();

    const parent_element =
        certManager.shadowRoot!.querySelector('#provisioned-client-certs');

    // <if expr="is_win or is_macosx">
    assertTrue(!!parent_element, 'parent element not found');
    const matchEls = parent_element.querySelectorAll('.cert-row');
    assertEquals(1, matchEls.length, 'no certs displayed');
    // TODO(crbug.com/40928765): test the displayed name/hash
    // </if>

    // <if expr="not (is_win or is_macosx)">
    // The provisioned client certs section should not be present on other OSes.
    assertFalse(!!parent_element, 'parent element was unexpectedly found');
    // </if>
  });
});
