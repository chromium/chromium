// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://certificate-manager/certificate_entry.js';

import type {CertificateEntryElement} from 'chrome://certificate-manager/certificate_entry.js';
import {CertificateSource} from 'chrome://certificate-manager/certificate_manager.mojom-webui.js';
import type {ActionResult} from 'chrome://certificate-manager/certificate_manager.mojom-webui.js';
import {CertificatesBrowserProxy} from 'chrome://certificate-manager/certificates_browser_proxy.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestCertificateManagerProxy} from './certificate_manager_test_support.js';

suite('CertificateEntryV2Test', () => {
  let certEntry: CertificateEntryElement;
  let testProxy: TestCertificateManagerProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestCertificateManagerProxy();
    CertificatesBrowserProxy.setInstance(testProxy);
  });

  function initializeElement() {
    certEntry = document.createElement('certificate-entry');
    certEntry.displayName = 'certname';
    certEntry.sha256hashHex = 'deadbeef';
    certEntry.certSource = CertificateSource.kChromeRootStore;
    certEntry.isDeletable = false;
    document.body.appendChild(certEntry);
  }

  test('element check', async () => {
    initializeElement();

    assertEquals(
        'deadbeef', certEntry.$.certhash.value, 'wrong hash in input box');
    assertFalse(isVisible(certEntry.$.delete));
    certEntry.$.view.click();
    const [source, hash] =
        await testProxy.handler.whenCalled('viewCertificate');
    assertEquals(
        CertificateSource.kChromeRootStore, source,
        'click provided wrong source');
    assertEquals('deadbeef', hash, 'click provided wrong hash');
    assertEquals('icon-visibility', certEntry.$.view.className);
  });

  test('edit icon', () => {
    certEntry = document.createElement('certificate-entry');
    certEntry.displayName = 'certname';
    certEntry.sha256hashHex = 'deadbeef';
    certEntry.certSource = CertificateSource.kChromeRootStore;
    certEntry.showEditIcon = true;
    document.body.appendChild(certEntry);

    assertEquals(
        'deadbeef', certEntry.$.certhash.value, 'wrong hash in input box');
    assertEquals('icon-edit', certEntry.$.view.className);
  });

  test('delete click', async () => {
    const kExpectedErrorMessage: string = 'test error message';
    const kExpectedDeleteResult: ActionResult = {error: kExpectedErrorMessage};
    testProxy.handler.setDeleteCertificateCallback(() => {
      return {result: kExpectedDeleteResult};
    });

    certEntry = document.createElement('certificate-entry');
    certEntry.displayName = 'certname';
    certEntry.sha256hashHex = 'deadbeef';
    certEntry.certSource = CertificateSource.kChromeRootStore;
    certEntry.isDeletable = true;

    let deleteResult: ActionResult|null = null;
    certEntry.addEventListener(
        'delete-result', (event: CustomEvent<ActionResult|null>) => {
          deleteResult = event.detail;
        });

    document.body.appendChild(certEntry);

    assertTrue(isVisible(certEntry.$.delete));
    certEntry.$.delete.click();
    const [source, name, hash] =
        await testProxy.handler.whenCalled('deleteCertificate');
    assertEquals(
        CertificateSource.kChromeRootStore, source,
        'click provided wrong source');
    assertEquals('deadbeef', hash, 'click provided wrong hash');
    assertEquals('certname', name, 'click provided wrong name');

    assertDeepEquals(deleteResult, kExpectedDeleteResult);
  });
});
