// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager v2 component.

import 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import 'chrome://settings/strings.m.js';

import type {CertificateEntryV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_entry_v2.js';
import type {CertificateManagerV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import type {CertificateManagerPageHandlerInterface, CertificateManagerPageRemote, SummaryCertInfo} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificateManagerPageCallbackRouter, CertificateSource} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificates_v2_browser_proxy.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';


class FakePageHandler extends TestBrowserProxy implements
    CertificateManagerPageHandlerInterface {
  private getCertificatesCallback_: (source: CertificateSource) => {
    certs:
      SummaryCertInfo[],
  } = (_) => {
    return {certs: []};
  };

  constructor() {
    super([
      'getCertificates',
      'viewCertificate',
      'exportChromeRootStore',
    ]);
  }

  getCertificates(source: CertificateSource):
      Promise<{certs: SummaryCertInfo[]}> {
    this.methodCalled('getCertificates', source);
    return Promise.resolve(this.getCertificatesCallback_(source));
  }

  viewCertificate(source: CertificateSource, sha256hashHex: string) {
    this.methodCalled('viewCertificate', source, sha256hashHex);
  }

  exportChromeRootStore() {
    this.methodCalled('exportChromeRootStore');
  }

  setCertificatesCallback(callbackFn: (source: CertificateSource) => {
    certs: SummaryCertInfo[],
  }) {
    this.getCertificatesCallback_ = callbackFn;
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
    certEntry.set('certSource', CertificateSource.kChromeRootStore);
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


suite('CertificateManagerV2Test', () => {
  let certManager: CertificateManagerV2Element;
  let testProxy: TestCertificateManagerProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    await navigator.clipboard.writeText('');
    testProxy = new TestCertificateManagerProxy();
    CertificatesV2BrowserProxy.setInstance(testProxy);
  });

  function initializeElement() {
    certManager = document.createElement('certificate-manager-v2');
    document.body.appendChild(certManager);
  }

  test('Copy CRS hash', async () => {
    const getCertificatesResolver = new PromiseResolver<void>();
    testProxy.handler.setCertificatesCallback((source: CertificateSource) => {
      if (source === CertificateSource.kChromeRootStore) {
        getCertificatesResolver.resolve();
        return {
          certs: [
            {
              sha256hashHex: 'deadbeef',
              displayName: 'cert1',
            },
          ],
        };
      }
      return {certs: []};
    });
    initializeElement();

    await getCertificatesResolver.promise;
    await microtasksFinished();
    assertFalse(certManager.$.toast.open);

    const certEntries =
        certManager.$.crsCerts.querySelectorAll('certificate-entry-v2');
    assertEquals(1, certEntries.length, 'no certs found');
    assertEquals('', await navigator.clipboard.readText());
    certEntries[0]!.$.copy.click();
    assertTrue(certManager.$.toast.open);
    assertEquals('deadbeef', await navigator.clipboard.readText());
  });

  test('CRS list populated', async () => {
    const getCertificatesResolver = new PromiseResolver<void>();
    testProxy.handler.setCertificatesCallback((source: CertificateSource) => {
      if (source === CertificateSource.kChromeRootStore) {
        getCertificatesResolver.resolve();
        return {
          certs: [
            {
              sha256hashHex: 'deadbeef',
              displayName: 'cert1',
            },
          ],
        };
      }
      return {certs: []};
    });
    initializeElement();

    await getCertificatesResolver.promise;
    await microtasksFinished();

    const matchEls =
        certManager.$.crsCerts.querySelectorAll('certificate-entry-v2');
    assertEquals(1, matchEls.length, 'no certs displayed');
    assertEquals('cert1', matchEls[0]!.displayName);
    assertEquals('deadbeef', matchEls[0]!.sha256hashHex);
  });

  test('Export CRS certs', async () => {
    const getCertificatesResolver = new PromiseResolver<void>();
    testProxy.handler.setCertificatesCallback((source: CertificateSource) => {
      if (source === CertificateSource.kChromeRootStore) {
        getCertificatesResolver.resolve();
        return {
          certs: [
            {
              sha256hashHex: 'deadbeef',
              displayName: 'cert1',
            },
          ],
        };
      }
      return {certs: []};
    });
    initializeElement();

    await getCertificatesResolver.promise;
    await microtasksFinished();
    assertFalse(certManager.$.toast.open);

    const certEntries =
        certManager.$.crsCerts.querySelectorAll('certificate-entry-v2');
    assertEquals(1, certEntries.length, 'no certs found');
    certManager.$.exportCRS.click();
    await testProxy.handler.whenCalled('exportChromeRootStore');
  });

  test('platform client certs populated', async () => {
    const getCertificatesResolver = new PromiseResolver<void>();
    testProxy.handler.setCertificatesCallback((source: CertificateSource) => {
      if (source === CertificateSource.kPlatformClientCert) {
        getCertificatesResolver.resolve();
        return {
          certs: [
            {
              sha256hashHex: 'deadbeef2',
              displayName: 'cert2',
            },
          ],
        };
      }
      return {certs: []};
    });
    initializeElement();

    await getCertificatesResolver.promise;
    await microtasksFinished();
    assertFalse(certManager.$.toast.open);

    const parentElement =
        certManager.shadowRoot!.querySelector('#platform-client-certs');
    assertTrue(!!parentElement, 'parent element not found');
    const matchEls = parentElement.querySelectorAll('certificate-entry-v2');
    assertEquals(1, matchEls.length, 'no certs displayed');
    assertEquals('cert2', matchEls[0]!.displayName);
    assertEquals('deadbeef2', matchEls[0]!.sha256hashHex);

    assertEquals('', await navigator.clipboard.readText());
    matchEls[0]!.$.copy.click();
    assertTrue(certManager.$.toast.open);
    assertEquals('deadbeef2', await navigator.clipboard.readText());
  });

  // <if expr="is_win or is_macosx">
  test('provisioned client certs populated', async () => {
    const getCertificatesResolver = new PromiseResolver<void>();
    testProxy.handler.setCertificatesCallback((source: CertificateSource) => {
      if (source === CertificateSource.kProvisionedClientCert) {
        getCertificatesResolver.resolve();
        return {
          certs: [
            {
              sha256hashHex: 'deadbeef3',
              displayName: 'cert3',
            },
          ],
        };
      }
      return {certs: []};
    });

    initializeElement();
    await getCertificatesResolver.promise;
    await microtasksFinished();
    assertFalse(certManager.$.toast.open);

    const parentElement =
        certManager.shadowRoot!.querySelector('#provisioned-client-certs');

    assertTrue(!!parentElement, 'parent element not found');
    const matchEls = parentElement.querySelectorAll('certificate-entry-v2');
    assertEquals(1, matchEls.length, 'no certs displayed');
    assertEquals('cert3', matchEls[0]!.displayName);
    assertEquals('deadbeef3', matchEls[0]!.sha256hashHex);

    assertEquals('', await navigator.clipboard.readText());
    matchEls[0]!.$.copy.click();
    assertTrue(certManager.$.toast.open);
    assertEquals('deadbeef3', await navigator.clipboard.readText());
  });
  // </if>

  // <if expr="not (is_win or is_macosx)">
  test('provisioned client certs not present', async () => {
    initializeElement();
    await microtasksFinished();

    const parentElement =
        certManager.shadowRoot!.querySelector('#provisioned-client-certs');
    // The provisioned client certs section should not be present on other OSes.
    assertFalse(!!parentElement, 'parent element was unexpectedly found');
  });
  // </if>
});
