// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/move_confirmation_page.js';

import {DialogPage} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {MoveConfirmationPageElement} from 'chrome://cloud-upload/move_confirmation_page.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {CloudUploadTestBrowserProxy, ProxyOptions} from './cloud_upload_test_browser_proxy.js';

suite('<move-confirmation-page>', () => {
  /* Holds the <move-confirmation-page> app. */
  let container: HTMLDivElement;
  /* The <move-confirmation-page> app. */
  let moveConfirmationPageApp: MoveConfirmationPageElement;
  /* The BrowserProxy element to make assertions on when mojo methods are
     called. */
  let testProxy: CloudUploadTestBrowserProxy;

  async function setUp(options: ProxyOptions) {
    testProxy = new CloudUploadTestBrowserProxy(options);
    CloudUploadBrowserProxy.setInstance(testProxy);

    // Creates and attaches the <move-confirmation-page> element to the DOM
    // tree.
    moveConfirmationPageApp =
        document.createElement('move-confirmation-page') as
        MoveConfirmationPageElement;
    container.appendChild(moveConfirmationPageApp);
    await moveConfirmationPageApp.init;
  }

  /**
   * Runs prior to all the tests running, attaches a div to enable isolated
   * clearing and attaching of the web component.
   */
  suiteSetup(() => {
    container = document.createElement('div');
    document.body.appendChild(container);
  });

  /**
   * Runs after each test. Removes all elements from the <div> that holds
   * the <move-confirmation-page> component.
   */
  teardown(() => {
    container.innerHTML = '';
    testProxy.handler.reset();
  });


  /**
   * Test that the checkbox does not appear if the move confirmation page has
   * not been shown before.
   */
  test('NoCheckboxBeforeFirstMoveConfirmation', async () => {
    await setUp({
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
      officeMoveConfirmationShown: false,
    });
    const hasCheckbox = moveConfirmationPageApp.$<CrCheckboxElement>(
                            '#always-move-checkbox') !== null;
    assertFalse(hasCheckbox);
  });

  /**
   * Test that the checkbox does appear if the move confirmation page has been
   * shown before.
   */
  test('CheckboxAfterMoveConfirmation', async () => {
    await setUp({
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
      officeMoveConfirmationShown: true,
    });
    const hasCheckbox = moveConfirmationPageApp.$<CrCheckboxElement>(
                            '#always-move-checkbox') !== null;
    assertTrue(hasCheckbox);
  });
});