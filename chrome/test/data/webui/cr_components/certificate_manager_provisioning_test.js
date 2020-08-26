// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/strings.m.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_details_dialog.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_entry.js';
import 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_list.js';

import {CertificateProvisioningActionEventDetail, CertificateProvisioningViewDetailsActionEvent} from 'chrome://resources/cr_components/certificate_manager/certificate_manager_types.js';
import {CertificateProvisioningBrowserProxy, CertificateProvisioningBrowserProxyImpl, CertificateProvisioningProcess} from 'chrome://resources/cr_components/certificate_manager/certificate_provisioning_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';
import {eventToPromise} from '../test_util.m.js';


/**
 * A test version of CertificateProvisioningBrowserProxy.
 * Provides helper methods for allowing tests to know when a method was called,
 * as well as specifying mock responses.
 *
 * @implements {CertificateProvisioningBrowserProxy}
 */
class TestCertificateProvisioningBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'refreshCertificateProvisioningProcesses',
      'triggerCertificateProvisioningProcessUpdate',
    ]);
  }

  /** override */
  refreshCertificateProvisioningProcesses() {
    this.methodCalled('refreshCertificateProvisioningProcesses');
  }

  /** override */
  triggerCertificateProvisioningProcessUpdate(certProfileId, isDeviceWide) {
    this.methodCalled(
        'triggerCertificateProvisioningProcessUpdate',
        {certProfileId, isDeviceWide});
  }
}

/** @return {!CertificateProvisioningProcess} */
function createSampleCertificateProvisioningProcess() {
  return {
    certProfileId: 'dummyProfileId',
    isDeviceWide: true,
    publicKey: 'dummyPublicKey',
    stateId: 8,
    status: 'dummyStateName',
    timeSinceLastUpdate: 'dummyTimeSinceLastUpdate',
  };
}

/**
 * @param {?CertificateProvisioningListElement} certProvisioningList
 * @return {!NodeList<!Element>}
 */
function getEntries(certProvisioningList) {
  assertTrue(!!certProvisioningList);
  return certProvisioningList.shadowRoot.querySelectorAll(
      'certificate-provisioning-entry');
}

suite('CertificateProvisioningEntryTests', function() {
  /** @type {!CertificateProvisioningEntryElement} */
  let entry;

  /** @type {?TestCertificateProvisioningBrowserProxy} */
  let browserProxy = null;

  /**
   * @return {!Promise} A promise firing once
   *     |CertificateProvisioningViewDetailsActionEvent| fires.
   */
  function actionEventToPromise() {
    return eventToPromise(CertificateProvisioningViewDetailsActionEvent, entry);
  }

  setup(function() {
    browserProxy = new TestCertificateProvisioningBrowserProxy();
    CertificateProvisioningBrowserProxyImpl.instance_ = browserProxy;
    entry = /** @type {!CertificateProvisioningEntryElement} */ (
        document.createElement('certificate-provisioning-entry'));
    entry.model = createSampleCertificateProvisioningProcess();
    document.body.appendChild(entry);

    // Bring up the popup menu for the following tests to use.
    entry.$$('#dots').click();
    flush();
  });

  teardown(function() {
    entry.remove();
  });

  // Test case where 'Details' option is tapped.
  test('MenuOptions_Details', function() {
    const detailsButton = entry.$$('#details');
    const waitForActionEvent = actionEventToPromise();
    detailsButton.click();
    return waitForActionEvent.then(function(event) {
      const detail =
          /** @type {!CertificateProvisioningActionEventDetail} */ (
              event.detail);
      assertEquals(entry.model, detail.model);
    });
  });
});

suite('CertificateManagerProvisioningTests', function() {
  /** @type {?CertificateProvisioningListElement} */
  let certProvisioningList = null;

  /** @type {?TestCertificateProvisioningBrowserProxy} */
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestCertificateProvisioningBrowserProxy();
    CertificateProvisioningBrowserProxyImpl.instance_ = browserProxy;
    certProvisioningList =
        /** @type {!CertificateProvisioningListElement} */ (
            document.createElement('certificate-provisioning-list'));
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
          webUIListenerCallback(
              'certificate-provisioning-processes-changed',
              [createSampleCertificateProvisioningProcess()]);

          flush();

          assertEquals(1, getEntries(certProvisioningList).length);
        });
  });

  test('OpensDialog_ViewDetails', function() {
    const dialogId = 'certificate-provisioning-details-dialog';
    const anchorForTest = document.createElement('a');
    document.body.appendChild(anchorForTest);

    assertFalse(!!certProvisioningList.$$(dialogId));
    const whenDialogOpen =
        eventToPromise('cr-dialog-open', certProvisioningList);
    certProvisioningList.fire(
        CertificateProvisioningViewDetailsActionEvent,
        /** @type {!CertificateProvisioningActionEventDetail} */ ({
          model: createSampleCertificateProvisioningProcess(),
          anchor: anchorForTest
        }));

    return whenDialogOpen
        .then(() => {
          const dialog =
              /** @type {!CertificateProvisioningDetailsDialogElement} */ (
                  certProvisioningList.$$(dialogId));
          assertTrue(!!dialog);
          const whenDialogClosed = eventToPromise('close', dialog);
          dialog.$$('#dialog').$$('#close').click();
          return whenDialogClosed;
        })
        .then(() => {
          const dialog = certProvisioningList.$$(dialogId);
          assertFalse(!!dialog);
        });
  });
});

suite('DetailsDialogTests', function() {
  /** @type {?CertificateProvisioningDetailsDialogElement} */
  let dialog = null;

  /** @type {?TestCertificateProvisioningBrowserProxy} */
  let browserProxy = null;

  setup(async function() {
    browserProxy = new TestCertificateProvisioningBrowserProxy();

    CertificateProvisioningBrowserProxyImpl.instance_ = browserProxy;
    dialog = /** @type {!CertificateProvisioningDetailsDialogElement} */ (
        document.createElement('certificate-provisioning-details-dialog'));
  });

  teardown(function() {
    dialog.remove();
  });

  test('RefreshProcess', function() {
    dialog.model = createSampleCertificateProvisioningProcess();
    document.body.appendChild(dialog);

    // Simulate clicking 'Refresh'.
    dialog.$.refresh.click();

    browserProxy.whenCalled('triggerCertificateProvisioningProcessUpdate')
        .then(function({certProfileId, isDeviceWide}) {
          assertEquals(dialog.model.certProfileId, certProfileId);
          assertEquals(dialog.model.isDeviceWide, isDeviceWide);
          // Check that the dialog is still open.
          assertTrue(dialog.$$('#dialog').open);
        });
  });
});
