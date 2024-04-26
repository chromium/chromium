// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager v2 component.

import 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';

import type {CertificateManagerV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import type {CertificateManagerPageHandlerInterface, CertificateManagerPageRemote, SummaryCertInfo} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificateManagerPageCallbackRouter} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificates_v2_browser_proxy.js';
import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';


class FakePageHandler extends TestBrowserProxy implements
    CertificateManagerPageHandlerInterface {
  private crsCerts_: SummaryCertInfo[] = [];

  constructor() {
    super([
      'getChromeRootStoreCerts',
    ]);
  }

  async getChromeRootStoreCerts(): Promise<{crsCertInfos: SummaryCertInfo[]}> {
    this.methodCalled('getChromeRootStoreCerts');
    return {'crsCertInfos': this.crsCerts_.slice()};
  }

  setChromeRootStoreCerts(crsCerts: SummaryCertInfo[]) {
    this.crsCerts_ = crsCerts;
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

    // TODO(crbug.com/40928765): Change test to actually look at the DOM as
    // opposed to the data stored in the element.
    assertDeepEquals(
        certs, certManager.crsCertificates, 'expected cert not present.');
  });
});
