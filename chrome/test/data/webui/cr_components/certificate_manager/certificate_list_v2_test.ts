// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/certificate_manager/certificate_list_v2.js';
import 'chrome://certificate-manager/strings.m.js';

import type {CertificateListV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_list_v2.js';
import {CertificateSource} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificates_v2_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestCertificateManagerProxy} from './certificate_manager_v2_test_support.js';

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
    certList.certSource = CertificateSource.kChromeRootStore;
    document.body.appendChild(certList);
  }

  test('element check', async () => {
    testProxy.handler.setCertificatesCallback(() => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
            isDeletable: false,
          },
          {
            sha256hashHex: 'deadbeef2',
            displayName: 'cert2',
            isDeletable: false,
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

    const entries = certList.$.certs.querySelectorAll('certificate-entry-v2');
    assertEquals(2, entries.length, 'no certs displayed');
    assertEquals('cert1', entries[0]!.displayName);
    assertEquals('deadbeef1', entries[0]!.sha256hashHex);
    assertEquals('cert2', entries[1]!.displayName);
    assertEquals('deadbeef2', entries[1]!.sha256hashHex);

    assertFalse(isVisible(certList.$.noCertsRow));
    assertTrue(isVisible(certList.$.expandButton));
  });

  test('collapse hidden', async () => {
    testProxy.handler.setCertificatesCallback(() => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
            isDeletable: false,
          },
          {
            sha256hashHex: 'deadbeef2',
            displayName: 'cert2',
            isDeletable: false,
          },
        ],
      };
    });

    certList = document.createElement('certificate-list-v2');
    certList.certSource = CertificateSource.kChromeRootStore;
    certList.noCollapse = true;
    document.body.appendChild(certList);

    assertEquals(
        CertificateSource.kChromeRootStore,
        await testProxy.handler.whenCalled('getCertificates'),
        'getCertificates called with wrong source');
    await microtasksFinished();

    const entries = certList.$.certs.querySelectorAll('certificate-entry-v2');
    assertEquals(2, entries.length, 'no certs displayed');

    assertFalse(isVisible(certList.$.noCertsRow));
    assertFalse(isVisible(certList.$.expandButton));
  });

  test('export', async () => {
    testProxy.handler.setCertificatesCallback(() => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
            isDeletable: false,
          },
          {
            sha256hashHex: 'deadbeef2',
            displayName: 'cert2',
            isDeletable: false,
          },
        ],
      };
    });
    initializeElement();

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertTrue(isVisible(certList.$.exportCerts));

    certList.$.exportCerts.click();

    assertEquals(
        CertificateSource.kChromeRootStore,
        await testProxy.handler.whenCalled('exportCertificates'),
        'export click provided wrong source');
  });

  test('export click propagation', async () => {
    testProxy.handler.setCertificatesCallback(() => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
            isDeletable: false,
          },
          {
            sha256hashHex: 'deadbeef2',
            displayName: 'cert2',
            isDeletable: false,
          },
        ],
      };
    });
    initializeElement();

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertTrue(certList.$.certs.opened, 'list not opened');
    assertTrue(isVisible(certList.$.exportCerts));

    certList.$.exportCerts.click();

    assertEquals(
        CertificateSource.kChromeRootStore,
        await testProxy.handler.whenCalled('exportCertificates'),
        'export click provided wrong source');

    await microtasksFinished();
    // Check that list of certs is still opened after export button click.
    assertTrue(certList.$.certs.opened, 'list not opened after click');
  });


  test('export hidden', async () => {
    certList = document.createElement('certificate-list-v2');
    certList.certSource = CertificateSource.kChromeRootStore;
    certList.hideExport = true;
    document.body.appendChild(certList);

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertFalse(isVisible(certList.$.exportCerts));
  });

  test('import click', async () => {
    testProxy.handler.setCertificatesCallback(() => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
            isDeletable: false,
          },
        ],
      };
    });

    certList = document.createElement('certificate-list-v2');
    certList.certSource = CertificateSource.kChromeRootStore;
    certList.showImport = true;
    document.body.appendChild(certList);

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertTrue(certList.$.certs.opened, 'list not opened');
    assertTrue(isVisible(certList.$.importCert));
    assertFalse(isVisible(certList.$.importAndBindCert));

    certList.$.importCert.click();

    assertEquals(
        CertificateSource.kChromeRootStore,
        await testProxy.handler.whenCalled('importCertificate'),
        'import click provided wrong source');

    await microtasksFinished();
    // Check that list of certs is still opened after import button click.
    assertTrue(certList.$.certs.opened, 'list not opened after click');
  });

  test('importAndBind click', async () => {
    testProxy.handler.setCertificatesCallback(() => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
            isDeletable: false,
          },
        ],
      };
    });

    certList = document.createElement('certificate-list-v2');
    certList.certSource = CertificateSource.kChromeRootStore;
    certList.showImportAndBind = true;
    document.body.appendChild(certList);

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertTrue(certList.$.certs.opened, 'list not opened');
    assertFalse(isVisible(certList.$.importCert));
    assertTrue(isVisible(certList.$.importAndBindCert));

    certList.$.importAndBindCert.click();

    assertEquals(
        CertificateSource.kChromeRootStore,
        await testProxy.handler.whenCalled('importAndBindCertificate'),
        'import click provided wrong source');

    await microtasksFinished();
    // Check that list of certs is still opened after import button click.
    assertTrue(certList.$.certs.opened, 'list not opened after click');
  });

  test('import hidden', async () => {
    initializeElement();

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertFalse(isVisible(certList.$.importCert));
    assertFalse(isVisible(certList.$.importAndBindCert));
  });

  test('no certs', async () => {
    certList = document.createElement('certificate-list-v2');
    certList.certSource = CertificateSource.kChromeRootStore;
    document.body.appendChild(certList);

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertFalse(isVisible(certList.$.exportCerts));
    assertTrue(isVisible(certList.$.noCertsRow));
    assertFalse(isVisible(certList.$.expandButton));
    assertTrue(isVisible(certList.$.listHeader));
  });

  test('hideIfEmpty with no certs - hide everything', async () => {
    certList = document.createElement('certificate-list-v2');
    certList.hideIfEmpty = true;
    certList.certSource = CertificateSource.kChromeRootStore;
    document.body.appendChild(certList);

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertFalse(isVisible(certList.$.certs));
    assertFalse(isVisible(certList.$.listHeader));
  });

  test('hideIfEmpty with certs, no hiding', async () => {
    testProxy.handler.setCertificatesCallback(() => {
      return {
        certs: [
          {
            sha256hashHex: 'deadbeef1',
            displayName: 'cert1',
            isDeletable: false,
          },
          {
            sha256hashHex: 'deadbeef2',
            displayName: 'cert2',
            isDeletable: false,
          },
        ],
      };
    });

    certList = document.createElement('certificate-list-v2');
    // This should be ignored since the list isn't empty.
    certList.hideIfEmpty = true;
    certList.certSource = CertificateSource.kChromeRootStore;
    document.body.appendChild(certList);

    initializeElement();

    assertEquals(
        CertificateSource.kChromeRootStore,
        await testProxy.handler.whenCalled('getCertificates'),
        'getCertificates called with wrong source');
    await microtasksFinished();

    const entries = certList.$.certs.querySelectorAll('certificate-entry-v2');
    assertEquals(2, entries.length, 'no certs displayed');
    assertEquals('cert1', entries[0]!.displayName);
    assertEquals('deadbeef1', entries[0]!.sha256hashHex);
    assertEquals('cert2', entries[1]!.displayName);
    assertEquals('deadbeef2', entries[1]!.sha256hashHex);

    assertFalse(isVisible(certList.$.noCertsRow));
    assertTrue(isVisible(certList.$.expandButton));
    assertTrue(isVisible(certList.$.certs));
    assertTrue(isVisible(certList.$.listHeader));
  });

  test('hide header', async () => {
    certList = document.createElement('certificate-list-v2');
    certList.hideHeader = true;
    certList.certSource = CertificateSource.kChromeRootStore;
    document.body.appendChild(certList);

    await testProxy.handler.whenCalled('getCertificates');
    await microtasksFinished();

    assertTrue(isVisible(certList.$.certs));
    assertFalse(isVisible(certList.$.listHeader));
  });
});
