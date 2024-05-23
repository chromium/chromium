// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager v2 component.

import 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import 'chrome://settings/strings.m.js';

import type {CertificateEntryV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_entry_v2.js';
import type {CertificateListV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_list_v2.js';
import type {CertificateManagerV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import type {CertificateManagerPageHandlerInterface, CertificateManagerPageRemote, CertPolicyInfo, SummaryCertInfo} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
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

  private policyInfo_: CertPolicyInfo = {
    includeSystemTrustStore: true,
    isIncludeSystemTrustStoreManaged: false,
  };

  constructor() {
    super([
      'getCertificates',
      'getPolicyInformation',
      'viewCertificate',
      'exportCertificates',
      'showNativeManageCertificates',
    ]);
  }

  getCertificates(source: CertificateSource):
      Promise<{certs: SummaryCertInfo[]}> {
    this.methodCalled('getCertificates', source);
    return Promise.resolve(this.getCertificatesCallback_(source));
  }

  getPolicyInformation(): Promise<{policyInfo: CertPolicyInfo}> {
    this.methodCalled('getPolicyInformation');
    return Promise.resolve({policyInfo: this.policyInfo_});
  }

  viewCertificate(source: CertificateSource, sha256hashHex: string) {
    this.methodCalled('viewCertificate', source, sha256hashHex);
  }

  exportCertificates(source: CertificateSource) {
    this.methodCalled('exportCertificates', source);
  }

  setCertificatesCallback(callbackFn: (source: CertificateSource) => {
    certs: SummaryCertInfo[],
  }) {
    this.getCertificatesCallback_ = callbackFn;
  }

  setPolicyInformation(policyInfo: CertPolicyInfo) {
    this.policyInfo_ = policyInfo;
  }
  // <if expr="is_win or is_macosx">
  showNativeManageCertificates() {
    this.methodCalled('showNativeManageCertificates');
  }
  // </if>
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

suite('CertificateListV2Test', () => {
  let certList: CertificateListV2Element;
  let testProxy: TestCertificateManagerProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestCertificateManagerProxy();
    CertificatesV2BrowserProxy.setInstance(testProxy);
  });

  function initializeElement() {
    certList = document.createElement('certificate-list-v2');
    certList.set('certSource', CertificateSource.kChromeRootStore);
    document.body.appendChild(certList);
  }

  test('element check', async () => {
    testProxy.handler.setCertificatesCallback((_: CertificateSource) => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
          },
          {
            sha256hashHex: 'deadbeef2',
            displayName: 'cert2',
          },
        ],
      };
    });

    initializeElement();

    assertEquals(
        CertificateSource.kChromeRootStore,
        await testProxy.handler.whenCalled('getCertificates'),
        'getCertificates called with wrong source');
    await microtasksFinished();

    const matchEls = certList.$.certs.querySelectorAll('certificate-entry-v2');
    assertEquals(2, matchEls.length, 'no certs displayed');
    assertEquals('cert1', matchEls[0]!.displayName);
    assertEquals('deadbeef1', matchEls[0]!.sha256hashHex);
    assertEquals('cert2', matchEls[1]!.displayName);
    assertEquals('deadbeef2', matchEls[1]!.sha256hashHex);
  });

  test('export', async () => {
    initializeElement();

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertFalse(certList.$.exportCerts.hidden);

    certList.$.exportCerts.click();

    assertEquals(
        CertificateSource.kChromeRootStore,
        await testProxy.handler.whenCalled('exportCertificates'),
        'export click provided wrong source');
  });

  test('export hidden', async () => {
    certList = document.createElement('certificate-list-v2');
    certList.set('certSource', CertificateSource.kChromeRootStore);
    certList.set('hideExport', true);
    document.body.appendChild(certList);

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertTrue(certList.$.exportCerts.hidden);
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
        certManager.$.crsCerts.$.certs.querySelectorAll('certificate-entry-v2');
    assertEquals(1, certEntries.length, 'no certs displayed');
    assertEquals('', await navigator.clipboard.readText());
    certEntries[0]!.$.copy.click();
    assertTrue(certManager.$.toast.open);
    assertEquals('deadbeef', await navigator.clipboard.readText());
  });

  test('Copy platform client certs hash', async () => {
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

    const matchEls = certManager.$.platformClientCerts.$.certs.querySelectorAll(
        'certificate-entry-v2');
    assertEquals(1, matchEls.length, 'no certs displayed');

    assertEquals('', await navigator.clipboard.readText());
    matchEls[0]!.$.copy.click();
    assertTrue(certManager.$.toast.open);
    assertEquals('deadbeef2', await navigator.clipboard.readText());
  });

  // <if expr="is_win or is_macosx">
  test('Copy provisioned client certs hash', async () => {
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

    const matchEls =
        certManager.$.provisionedClientCerts.$.certs.querySelectorAll(
            'certificate-entry-v2');
    assertEquals(1, matchEls.length, 'no certs displayed');

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
        certManager.shadowRoot!.querySelector('#provisionedClientCerts');
    // The provisioned client certs section should not be present on other OSes.
    assertFalse(
        !!parentElement,
        'provisionedClientCerts element was unexpectedly found');
  });
  // </if>

  test('Policy - OS certs imported and managed', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: true,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();

    assertTrue(certManager.$.importOsCerts.checked, 'os toggle state wrong');
    assertFalse(
        certManager.$.importOsCertsManagedIcon.hidden,
        'enterprise managed icon visibility wrong');
    assertFalse(
        certManager.$.viewOsImportedCerts.hidden,
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertFalse(
        certManager.$.manageOsImportedCerts.hidden,
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs imported but not managed', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: false,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();

    assertTrue(
        certManager.$.importOsCerts.checked, 'os import toggle state wrong');
    assertTrue(
        certManager.$.importOsCertsManagedIcon.hidden,
        'enterprise managed icon visibility wrong');
    assertFalse(
        certManager.$.viewOsImportedCerts.hidden,
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertFalse(
        certManager.$.manageOsImportedCerts.hidden,
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs not imported but managed', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: false,
      isIncludeSystemTrustStoreManaged: true,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();

    assertFalse(
        certManager.$.importOsCerts.checked, 'os import toggle state wrong');
    assertFalse(
        certManager.$.importOsCertsManagedIcon.hidden,
        'enterprise managed icon visibility wrong');
    assertTrue(
        certManager.$.viewOsImportedCerts.hidden,
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertTrue(
        certManager.$.manageOsImportedCerts.hidden,
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs not imported and not managed', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: false,
      isIncludeSystemTrustStoreManaged: false,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();

    assertFalse(
        certManager.$.importOsCerts.checked, 'os import toggle state wrong');
    assertTrue(
        certManager.$.importOsCertsManagedIcon.hidden,
        'enterprise managed icon visibility wrong');
    assertTrue(
        certManager.$.viewOsImportedCerts.hidden,
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertTrue(
        certManager.$.manageOsImportedCerts.hidden,
        'imported os certs external link visibility wrong');
    // </if>
  });

  // <if expr="is_win or is_macosx">
  test('Open native certificate management', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: true,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();
    assertFalse(
        certManager.$.manageOsImportedCerts.hidden,
        'imported os certs external link visibility wrong');
    certManager.$.manageOsImportedCerts.click();
    await testProxy.handler.whenCalled('showNativeManageCertificates');
  });
  // </if>
});
