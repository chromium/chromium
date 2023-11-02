// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/new_tab_page.js';

import {LensUploadDialogElement} from 'chrome://new-tab-page/lazy_load.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {installMock} from './test_support.js';

suite('LensUploadDialogTest', () => {
  let uploadDialog: LensUploadDialogElement;
  let wrapperElement: HTMLDivElement;
  let outsideClickTarget: HTMLDivElement;
  let windowProxy: TestBrowserProxy;

  setup(() => {
    document.body.innerHTML = '';
    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('onLine', true);

    // Larger than wrapper so that we can test outside clicks.
    document.body.style.width = '1000px';

    wrapperElement = document.createElement('div');
    // Rough approximate size of the realbox.
    wrapperElement.style.width = '500px';
    wrapperElement.style.margin = '0 auto';
    document.body.appendChild(wrapperElement);

    // Click target to test outside clicks.
    outsideClickTarget = document.createElement('div');
    outsideClickTarget.style.width = '50px';
    outsideClickTarget.style.height = '50px';
    outsideClickTarget.style.border = '1px dashed red';
    document.body.appendChild(outsideClickTarget);

    uploadDialog = document.createElement('ntp-lens-upload-dialog');
    wrapperElement.appendChild(uploadDialog);
  });

  test('hidden be default', () => {
    // Assert.
    assertTrue(uploadDialog.$.dialog.hidden);
  });

  test('shows when openDialog is called', () => {
    // Act.
    uploadDialog.openDialog();

    // Assert.
    assertFalse(uploadDialog.$.dialog.hidden);
  });

  test('hides when close button is clicked', () => {
    // Arrange.
    uploadDialog.openDialog();

    // Act.
    const closeButton =
      uploadDialog.shadowRoot!.querySelector('#closeButton') as HTMLElement;
    closeButton.click();

    // Assert.
    assertTrue(uploadDialog.$.dialog.hidden);
  });

  test('clicking outside the upload dialog closes the dialog', async () => {
    // Arrange.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Act.
    outsideClickTarget.click();

    // Assert.
    assertTrue(uploadDialog.$.dialog.hidden);
  });

  test('opening dialog while offline shows offline UI', async () => {
    // Arrange.
    windowProxy.setResultFor('onLine', false);

    // Act.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Assert.
    assertTrue(uploadDialog.hasAttribute('is-offline_'));

    // Reset.
    windowProxy.setResultFor('onLine', true);
  });

  test(
      'clicking try again in offline state when online updates UI',
      async () => {
        // Arrange.
        windowProxy.setResultFor('onLine', false);

        // Act.
        uploadDialog.openDialog();
        await waitAfterNextRender(uploadDialog);

        // Assert. (consistency check)
        assertTrue(uploadDialog.hasAttribute('is-offline_'));

        // Arrange.
        windowProxy.setResultFor('onLine', true);

        // Act.
        (uploadDialog.shadowRoot!.querySelector('#offlineRetryButton') as
         HTMLElement)!.click();
        await waitAfterNextRender(uploadDialog);

        // Assert.
        assertFalse(uploadDialog.hasAttribute('is-offline_'));
      });
});
