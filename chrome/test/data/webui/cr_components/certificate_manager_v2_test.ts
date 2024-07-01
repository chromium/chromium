// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the certificate-manager v2 component.

import 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import 'chrome://certificate-manager/strings.m.js';

import type {CertificateManagerV2Element} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.js';
import type {CertPolicyInfo} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_v2.mojom-webui.js';
import {CertificatesV2BrowserProxy} from 'chrome://resources/cr_components/certificate_manager/certificates_v2_browser_proxy.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestCertificateManagerProxy} from './certificate_manager_v2_test_support.js';

suite('CertificateManagerV2Test', () => {
  let certManager: CertificateManagerV2Element;
  let testProxy: TestCertificateManagerProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestCertificateManagerProxy();
    CertificatesV2BrowserProxy.setInstance(testProxy);
  });

  function initializeElement() {
    certManager = document.createElement('certificate-manager-v2');
    document.body.appendChild(certManager);
  }

  // <if expr="not (is_win or is_macosx or is_linux)">
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
      numPolicyCerts: 0,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();

    assertTrue(certManager.$.importOsCerts.checked, 'os toggle state wrong');
    assertTrue(
        isVisible(certManager.$.importOsCertsManagedIcon),
        'enterprise managed icon visibility wrong');
    assertTrue(
        isVisible(certManager.$.viewOsImportedCerts),
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertTrue(
        isVisible(certManager.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs imported but not managed', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: false,
      numPolicyCerts: 0,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();

    assertTrue(
        certManager.$.importOsCerts.checked, 'os import toggle state wrong');
    assertFalse(
        isVisible(certManager.$.importOsCertsManagedIcon),
        'enterprise managed icon visibility wrong');
    assertTrue(
        isVisible(certManager.$.viewOsImportedCerts),
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertTrue(
        isVisible(certManager.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs not imported but managed', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: false,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 0,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();

    assertFalse(
        certManager.$.importOsCerts.checked, 'os import toggle state wrong');
    assertTrue(
        isVisible(certManager.$.importOsCertsManagedIcon),
        'enterprise managed icon visibility wrong');
    assertFalse(
        isVisible(certManager.$.viewOsImportedCerts),
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertFalse(
        isVisible(certManager.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs not imported and not managed', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: false,
      isIncludeSystemTrustStoreManaged: false,
      numPolicyCerts: 0,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();

    assertFalse(
        certManager.$.importOsCerts.checked, 'os import toggle state wrong');
    assertFalse(
        isVisible(certManager.$.importOsCertsManagedIcon),
        'enterprise managed icon visibility wrong');
    assertFalse(
        isVisible(certManager.$.viewOsImportedCerts),
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertFalse(
        isVisible(certManager.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    // </if>
  });

  // <if expr="is_win or is_macosx">
  test('Open native certificate management', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 0,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    certManager.$.localMenuItem.click();
    await microtasksFinished();
    assertTrue(
        isVisible(certManager.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    certManager.$.manageOsImportedCerts.click();
    await testProxy.handler.whenCalled('showNativeManageCertificates');
  });

  test('Open native client certificate management', async () => {
    initializeElement();
    certManager.$.clientMenuItem.click();

    await microtasksFinished();
    assertTrue(
        isVisible(certManager.$.manageOsImportedClientCerts),
        'imported os certs external link visibility wrong');
    certManager.$.manageOsImportedClientCerts.click();
    await testProxy.handler.whenCalled('showNativeManageCertificates');
  });
  // </if>

  test('no admin certs, hide custom section', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 0,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();
    const customSection =
        certManager.shadowRoot!.querySelector('#customCertsSection');
    assertNull(customSection, 'custom certs section not hidden');
  });

  test('have admin certs, show custom section', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 5,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();
    const customSection =
        certManager.shadowRoot!.querySelector('#customCertsSection');
    const linkRow = customSection!.querySelector('cr-link-row');
    assertEquals('5 certificates', linkRow!.subLabel);
  });

  test('show admin certs', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 5,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();
    const customSection =
        certManager.shadowRoot!.querySelector('#customCertsSection');
    const linkRow = customSection!.querySelector('cr-link-row');
    linkRow!.click();
    await microtasksFinished();
    assertTrue(
        certManager.$.adminCertsSection.classList.contains('iron-selected'));
  });

  test('navigate back from admin certs', async () => {
    const policyInfo: CertPolicyInfo = {
      includeSystemTrustStore: true,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 5,
    };
    testProxy.handler.setPolicyInformation(policyInfo);
    initializeElement();

    await testProxy.handler.whenCalled('getPolicyInformation');
    await microtasksFinished();
    const customSection =
        certManager.shadowRoot!.querySelector('#customCertsSection');
    const linkRow = customSection!.querySelector('cr-link-row');
    linkRow!.click();
    await microtasksFinished();
    assertTrue(
        certManager.$.adminCertsSection.classList.contains('iron-selected'));
    certManager.$.adminCertsSection.$.backButton.click();
    await microtasksFinished();
    assertFalse(
        certManager.$.adminCertsSection.classList.contains('iron-selected'));
    assertTrue(
        certManager.$.localCertSection.classList.contains('iron-selected'));
  });

  test('click local certs section', async () => {
    initializeElement();
    certManager.$.localMenuItem.click();
    await microtasksFinished();
    assertTrue(
        certManager.$.localCertSection.classList.contains('iron-selected'));
  });

  test('click client certs section', async () => {
    initializeElement();
    certManager.$.clientMenuItem.click();
    await microtasksFinished();
    assertTrue(
        certManager.$.clientCertSection.classList.contains('iron-selected'));
  });

  test('click crs certs section', async () => {
    initializeElement();
    certManager.$.crsMenuItem.click();
    await microtasksFinished();
    assertTrue(
        certManager.$.crsCertSection.classList.contains('iron-selected'));
  });
});
