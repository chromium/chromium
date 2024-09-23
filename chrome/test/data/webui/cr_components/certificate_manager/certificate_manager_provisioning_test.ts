// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/strings.m.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_list.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_entry.js';

import type {CertificateProvisioningActionEventDetail} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_types.js';
import {CertificateProvisioningViewDetailsActionEvent} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_types.js';
import type {CertificateProvisioningBrowserProxy, CertificateProvisioningProcess} from 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_browser_proxy.js';
import {CertificateProvisioningBrowserProxyImpl} from 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_browser_proxy.js';
import type {CertificateProvisioningDetailsDialogElement} from 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_details_dialog.js';
import type {CertificateProvisioningEntryElement} from 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_entry.js';
import type {CertificateProvisioningListElement} from 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_list.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';


/**
 * A test version of CertificateProvisioningBrowserProxy.
 * Provides helper methods for allowing tests to know when a method was called,
 * as well as specifying mock responses.
 */
class TestCertificateProvisioningBrowserProxy extends TestBrowserProxy
    implements CertificateProvisioningBrowserProxy {
  constructor() {
    super([
      'refreshCertificateProvisioningProcesses',
      'triggerCertificateProvisioningProcessUpdate',
      'triggerCertificateProvisioningProcessReset',
    ]);
  }

  refreshCertificateProvisioningProcesses() {
    this.methodCalled('refreshCertificateProvisioningProcesses');
  }

  triggerCertificateProvisioningProcessUpdate(certProfileId: string) {
    this.methodCalled(
        'triggerCertificateProvisioningProcessUpdate', certProfileId);
  }
  triggerCertificateProvisioningProcessReset(certProfileId: string) {
    this.methodCalled(
        'triggerCertificateProvisioningProcessReset', certProfileId);
  }
}

function createSampleCertificateProvisioningProcess(isUpdated: boolean):
    CertificateProvisioningProcess {
  return {
    certProfileId: 'dummyProfileId',
    certProfileName: 'Dummy Profile Name',
    isDeviceWide: true,
    publicKey: 'dummyPublicKey',
    stateId: 8,
    status: isUpdated ? 'dummyStateName2' : 'dummyStateName',
    timeSinceLastUpdate: 'dummyTimeSinceLastUpdate',
    lastUnsuccessfulMessage: 'dummyLastUnsuccessfulMessage',
  };
}

function getEntries(certProvisioningList: CertificateProvisioningListElement):
    NodeListOf<CertificateProvisioningEntryElement> {
  return certProvisioningList.shadowRoot!.querySelectorAll(
      'certificate-provisioning-entry');
}

suite('CertificateProvisioningEntryTests', function() {
  let entry: CertificateProvisioningEntryElement;
  let browserProxy: TestCertificateProvisioningBrowserProxy;

  function actionEventToPromise():
      Promise<CustomEvent<CertificateProvisioningActionEventDetail>> {
    return eventToPromise(CertificateProvisioningViewDetailsActionEvent, entry);
  }

  setup(function() {
    browserProxy = new TestCertificateProvisioningBrowserProxy();
    CertificateProvisioningBrowserProxyImpl.setInstance(browserProxy);
    entry = document.createElement('certificate-provisioning-entry');
    entry.model = createSampleCertificateProvisioningProcess(false);
    document.body.appendChild(entry);

    // Bring up the popup menu for the following tests to use.
    entry.$.dots.click();
    flush();
  });

  teardown(function() {
    entry.remove();
  });

  // Test case where 'Details' option is tapped.
  test('MenuOptions_Details', function() {
    const detailsButton =
        entry.shadowRoot!.querySelector<HTMLElement>('#details');
    assertTrue(!!detailsButton);
    const waitForActionEvent = actionEventToPromise();
    detailsButton.click();
    return waitForActionEvent.then(function(event) {
      assertEquals(entry.model, event.detail.model);
    });
  });
});

suite('CertificateManagerProvisioningTests', function() {
  let certProvisioningList: CertificateProvisioningListElement;
  let browserProxy: TestCertificateProvisioningBrowserProxy;

  setup(function() {
    browserProxy = new TestCertificateProvisioningBrowserProxy();
    CertificateProvisioningBrowserProxyImpl.setInstance(browserProxy);
    certProvisioningList =
        document.createElement('certificate-provisioning-list');
    document.body.appendChild(certProvisioningList);
  });

  teardown(function() {
    certProvisioningList.remove();
  });

  /**
   * Test that the certProvisioningList requests information about certificate
   * provisioning processesfrom the browser on startup and that it gets
   * populated accordingly.
   */
  test('Initialization', function() {
    assertEquals(0, getEntries(certProvisioningList).length);

    return browserProxy.whenCalled('refreshCertificateProvisioningProcesses')
        .then(function() {
          browserProxy.resetResolver('refreshCertificateProvisioningProcesses');
          webUIListenerCallback(
              'certificate-provisioning-processes-changed',
              [createSampleCertificateProvisioningProcess(false)]);

          flush();

          assertEquals(1, getEntries(certProvisioningList).length);
        });
  });

  test('OpensDialog_ViewDetails', function() {
    const dialogId = 'certificate-provisioning-details-dialog';
    const anchorForTest = document.createElement('a');
    document.body.appendChild(anchorForTest);

    assertFalse(!!certProvisioningList.shadowRoot!.querySelector(dialogId));
    const whenDialogOpen =
        eventToPromise('cr-dialog-open', certProvisioningList);
    certProvisioningList.dispatchEvent(
        new CustomEvent(CertificateProvisioningViewDetailsActionEvent, {
          bubbles: true,
          composed: true,
          detail: {
            model: createSampleCertificateProvisioningProcess(false),
            anchor: anchorForTest,
          },
        }));

    return whenDialogOpen
        .then(() => {
          const dialog =
              certProvisioningList.shadowRoot!.querySelector(dialogId);
          assertTrue(!!dialog);
          const whenDialogClosed = eventToPromise('close', dialog);
          dialog.$.dialog.shadowRoot!.querySelector<HTMLElement>(
                                         '#close')!.click();
          return whenDialogClosed;
        })
        .then(() => {
          const dialog =
              certProvisioningList.shadowRoot!.querySelector(dialogId);
          assertFalse(!!dialog);
        });
  });

  test('OpensDialog_RefreshesData', async function() {
    const dialogId = 'certificate-provisioning-details-dialog';
    const anchorForTest = document.createElement('a');
    document.body.appendChild(anchorForTest);
    assertFalse(!!certProvisioningList.shadowRoot!.querySelector(dialogId));
    certProvisioningList.dispatchEvent(
        new CustomEvent(CertificateProvisioningViewDetailsActionEvent, {
          bubbles: true,
          composed: true,
          detail: {
            model: createSampleCertificateProvisioningProcess(false),
            anchor: anchorForTest,
          },
        }));
    const whenRefreshCalled =
        browserProxy.whenCalled('refreshCertificateProvisioningProcesses');
    await whenRefreshCalled;
  });
});

suite('DetailsDialogTests', function() {
  let browserProxy: TestCertificateProvisioningBrowserProxy;
  let certProvisioningList: CertificateProvisioningListElement;
  let dialog: CertificateProvisioningDetailsDialogElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestCertificateProvisioningBrowserProxy();
    CertificateProvisioningBrowserProxyImpl.setInstance(browserProxy);

    certProvisioningList =
        document.createElement('certificate-provisioning-list');
    document.body.appendChild(certProvisioningList);

    const anchorForTest = document.createElement('a');
    document.body.appendChild(anchorForTest);

    // Open the details dialog for testing.
    const dialogId = 'certificate-provisioning-details-dialog';
    assertFalse(!!certProvisioningList.shadowRoot!.querySelector(dialogId));
    const whenDialogOpen =
        eventToPromise('cr-dialog-open', certProvisioningList);
    certProvisioningList.dispatchEvent(
        new CustomEvent(CertificateProvisioningViewDetailsActionEvent, {
          bubbles: true,
          composed: true,
          detail: {
            model: createSampleCertificateProvisioningProcess(false),
            anchor: anchorForTest,
          },
        }));
    await whenDialogOpen;
    dialog = certProvisioningList.shadowRoot!.querySelector(dialogId)!;
    // Check if the dialog is initialized and opened.
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
  });

  test('RefreshProcess', async function() {
    // Simulate clicking 'Refresh'.
    dialog.$.refresh.click();

    const certProfileId = await browserProxy.whenCalled(
        'triggerCertificateProvisioningProcessUpdate');
    // Check if the parameters received by function are correct.
    assertEquals(dialog.model.certProfileId, certProfileId);
    // Check that the dialog is still open.
    assertTrue(dialog.$.dialog.open);
  });

  /**
   * Test that the details dialog gets updated correctly if the process is
   * changed after refreshCertificateProvisioningProcesses.
   */
  test('UpdateDialogDetails', async function() {
    // Start testing when dialog is open.
    await browserProxy.whenCalled('refreshCertificateProvisioningProcesses');

    // Check the status of dialog.model.
    assertEquals(dialog.model.status, 'dummyStateName');
    webUIListenerCallback(
        'certificate-provisioning-processes-changed',
        [createSampleCertificateProvisioningProcess(true)]);
    flush();
    // Check if the status of dialog.model is updated accordingly.
    assertEquals(dialog.model.status, 'dummyStateName2');
  });

  /**
   * Test that the details dialog gets closed if the process doesn't exist in
   * the list after refreshCertificateProvisioningProcesses.
   */
  test('CloseDialog', async function() {
    await browserProxy.whenCalled('refreshCertificateProvisioningProcesses');

    webUIListenerCallback('certificate-provisioning-processes-changed', []);
    flush();
    // Check that the dialog closes if the process no longer exists.
    assertFalse(dialog.$.dialog.open);
  });
});
