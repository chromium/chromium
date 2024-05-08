// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager v2 component.

import 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';

import type {CertificateManagerV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import type {CertificateManagerPageHandlerInterface, CertificateManagerPageRemote, SummaryCertInfo} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificateManagerPageCallbackRouter} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificates_v2_browser_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
// <if expr="not (is_win or is_macosx)">
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
// </if>
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


  test('CRS list populated', async () => {
    const certs: SummaryCertInfo[] = [
      {
        'sha256hashHex': 'deadbeef',
        'displayName': 'cert1',
      },
    ];
    testProxy.handler.setChromeRootStoreCerts(certs);
    initializeElement();

    await microtasksFinished();

    const matchEls = certManager.$.crsCerts.querySelectorAll('.cert-row');
    assertEquals(1, matchEls.length, 'no certs displayed');
    const inputs = certManager.shadowRoot!.querySelectorAll('cr-input');
    assertEquals(1, inputs.length, 'no inputs found');
    assertEquals('deadbeef', inputs[0]!.value);
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
