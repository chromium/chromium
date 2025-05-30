// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These test suites test the local-certs-section component.

import 'chrome://certificate-manager/local_certs_section.js';

import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {CertManagementMetadata} from 'chrome://certificate-manager/certificate_manager.mojom-webui.js';
import {CertificatesBrowserProxy} from 'chrome://certificate-manager/certificates_browser_proxy.js';
import type {LocalCertsSectionElement} from 'chrome://certificate-manager/local_certs_section.js';
import {assertEquals, assertNull} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// <if expr="not is_chromeos">
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// </if>

import {TestCertificateManagerProxy} from './certificate_manager_test_support.js';

class CertManagerTestPluralStringProxy extends TestPluralStringProxy {
  override text: string = '';

  override getPluralString(messageName: string, itemCount: number) {
    if (messageName === 'certificateManagerV2NumCerts') {
      this.methodCalled('getPluralString', {messageName, itemCount});
    }
    return Promise.resolve(this.text);
  }
}

suite('LocalCertsSectionV2Test', () => {
  let localCertsSection: LocalCertsSectionElement;
  let testProxy: TestCertificateManagerProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestCertificateManagerProxy();
    CertificatesBrowserProxy.setInstance(testProxy);
  });

  function initializeElement() {
    localCertsSection = document.createElement('local-certs-section');
    document.body.appendChild(localCertsSection);
  }

  // <if expr="not is_chromeos">
  test('Policy - OS certs number string', async () => {
    const pluralStringProxy = new CertManagerTestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);
    pluralStringProxy.text = '5 certificates';
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 5,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await pluralStringProxy.whenCalled('getPluralString');
    await microtasksFinished();

    assertEquals(
        '5 certificates', localCertsSection.$.numSystemCerts.innerText,
        'num system certs string incorrect');
    assertTrue(isVisible(localCertsSection.$.numSystemCerts));
  });
  // </if>

  // <if expr="not is_chromeos">
  test('Policy - OS certs imported and managed', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 4,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();

    assertTrue(
        localCertsSection.$.importOsCerts.checked, 'os toggle state wrong');
    assertTrue(
        isVisible(localCertsSection.$.importOsCertsManagedIcon),
        'enterprise managed icon visibility wrong');
    assertTrue(
        isVisible(localCertsSection.$.viewOsImportedCerts),
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertTrue(
        isVisible(localCertsSection.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs imported but not managed', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 4,
      isIncludeSystemTrustStoreManaged: false,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();

    assertTrue(
        localCertsSection.$.importOsCerts.checked,
        'os import toggle state wrong');
    assertFalse(
        isVisible(localCertsSection.$.importOsCertsManagedIcon),
        'enterprise managed icon visibility wrong');
    assertTrue(
        isVisible(localCertsSection.$.viewOsImportedCerts),
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertTrue(
        isVisible(localCertsSection.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs not imported but managed', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: false,
      numUserAddedSystemCerts: 4,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();

    assertFalse(
        localCertsSection.$.importOsCerts.checked,
        'os import toggle state wrong');
    assertTrue(isVisible(localCertsSection.$.numSystemCerts));
    assertTrue(
        isVisible(localCertsSection.$.importOsCertsManagedIcon),
        'enterprise managed icon visibility wrong');
    assertTrue(
        isVisible(localCertsSection.$.viewOsImportedCerts),
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertFalse(
        isVisible(localCertsSection.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('Policy - OS certs not imported and not managed', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: false,
      numUserAddedSystemCerts: 3,
      isIncludeSystemTrustStoreManaged: false,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();

    assertFalse(
        localCertsSection.$.importOsCerts.checked,
        'os import toggle state wrong');
    assertTrue(isVisible(localCertsSection.$.numSystemCerts));
    assertFalse(
        isVisible(localCertsSection.$.importOsCertsManagedIcon),
        'enterprise managed icon visibility wrong');
    assertTrue(
        isVisible(localCertsSection.$.viewOsImportedCerts),
        'view imported os certs link visibility wrong');
    // <if expr="is_win or is_macosx">
    assertFalse(
        isVisible(localCertsSection.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    // </if>
  });

  test('view OS certs not visible when 0 certs', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: false,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: false,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();

    assertFalse(
        isVisible(localCertsSection.$.viewOsImportedCerts),
        'view imported os certs should not be visible w/ 0 certs');
  });
  // </if>

  // <if expr="is_win or is_macosx">
  test('Open native certificate management', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();
    assertTrue(
        isVisible(localCertsSection.$.manageOsImportedCerts),
        'imported os certs external link visibility wrong');
    localCertsSection.$.manageOsImportedCerts.click();
    await testProxy.handler.whenCalled('showNativeManageCertificates');
  });
  // </if>

  test('no admin certs, hide admin certs section', async () => {
    const metadata: CertManagementMetadata = {
      // <if expr="not is_chromeos">
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: true,
      // </if>
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: true,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();
    const customSection =
        localCertsSection.shadowRoot!.querySelector('#customCertsSection');
    const adminLinkRow =
        customSection!.querySelector('#adminCertsInstalledLinkRow');
    assertNull(adminLinkRow, 'admin certs section not hidden');
  });

  test('have admin certs, show admin certs section', async () => {
    const pluralStringProxy = new CertManagerTestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);
    pluralStringProxy.text = '5 certificates';
    const metadata: CertManagementMetadata = {
      // <if expr="not is_chromeos">
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: true,
      // </if>
      numPolicyCerts: 5,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await pluralStringProxy.whenCalled('getPluralString');
    await microtasksFinished();
    const customSection =
        localCertsSection.shadowRoot!.querySelector('#customCertsSection');
    const adminLinkRow = customSection!.querySelector('cr-link-row');
    assertEquals('5 certificates', adminLinkRow!.subLabel);
  });

  // <if expr="not is_chromeos">
  test('os certs set by policy, disable OS certs toggle', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: true,
      numPolicyCerts: 5,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();
    assertTrue(localCertsSection.$.importOsCerts.disabled);
  });

  test('os certs not set by policy, enable OS certs toggle', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: false,
      numPolicyCerts: 5,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();
    assertFalse(localCertsSection.$.importOsCerts.disabled);
  });
  // </if>

  // <if expr="not is_chromeos">
  test('import OS certs toggle disable', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: false,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();

    localCertsSection.$.importOsCerts.click();
    await testProxy.handler.whenCalled('setIncludeSystemTrustStore');
    assertFalse(testProxy.handler.getArgs('setIncludeSystemTrustStore')[0]);
  });

  test('import OS certs toggle enable', async () => {
    const metadata: CertManagementMetadata = {
      includeSystemTrustStore: false,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: false,
      numPolicyCerts: 0,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();

    localCertsSection.$.importOsCerts.click();
    await testProxy.handler.whenCalled('setIncludeSystemTrustStore');
    assertTrue(testProxy.handler.getArgs('setIncludeSystemTrustStore')[0]);
  });
  // </if>

  test('user certs write enabled, show user certs section', async () => {
    const pluralStringProxy = new CertManagerTestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);
    pluralStringProxy.text = '5 certificates';
    const metadata: CertManagementMetadata = {
      // <if expr="not is_chromeos">
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: true,
      // </if>
      numPolicyCerts: 5,
      numUserCerts: 5,
      showUserCertsUi: true,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();
    const customSection =
        localCertsSection.shadowRoot!.querySelector('#customCertsSection');
    const userLinkDiv = customSection!.querySelector('#userCertsSection');
    const userLinkRow = userLinkDiv!.querySelector('cr-link-row');
    assertEquals('5 certificates', userLinkRow!.subLabel);
  });

  test('user certs write disabled, hide user certs section', async () => {
    const metadata: CertManagementMetadata = {
      // <if expr="not is_chromeos">
      includeSystemTrustStore: true,
      numUserAddedSystemCerts: 0,
      isIncludeSystemTrustStoreManaged: true,
      // </if>
      numPolicyCerts: 1,
      numUserCerts: 0,
      showUserCertsUi: false,
    };
    testProxy.handler.setCertManagementMetadata(metadata);
    initializeElement();

    await testProxy.handler.whenCalled('getCertManagementMetadata');
    await microtasksFinished();
    const customSection =
        localCertsSection.shadowRoot!.querySelector('#customCertsSection');
    const userLinkRow =
        customSection!.querySelector('#userCertsInstalledLinkRow');
    assertNull(userLinkRow, 'user certs section not hidden');
  });

  test(
      'no admin certs, user certs write disabled, hide custom certs section',
      async () => {
        const metadata: CertManagementMetadata = {
          // <if expr="not is_chromeos">
          includeSystemTrustStore: true,
          numUserAddedSystemCerts: 0,
          isIncludeSystemTrustStoreManaged: true,
          // </if>
          numPolicyCerts: 0,
          numUserCerts: 0,
          showUserCertsUi: false,
        };
        testProxy.handler.setCertManagementMetadata(metadata);
        initializeElement();

        await testProxy.handler.whenCalled('getCertManagementMetadata');
        await microtasksFinished();
        const customSection =
            localCertsSection.shadowRoot!.querySelector('#customCertsSection');
        assertNull(customSection, 'custom certs section not hidden');
      });
});
