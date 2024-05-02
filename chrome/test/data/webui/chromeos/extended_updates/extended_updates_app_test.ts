// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extended-updates-dialog/app.js';

import {ExtendedUpdatesAppElement} from 'chrome://extended-updates-dialog/app.js';
import {ExtendedUpdatesBrowserProxy} from 'chrome://extended-updates-dialog/extended_updates_browser_proxy.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestExtendedUpdatesBrowserProxy} from './test_extended_updates_browser_proxy.js';

suite('<extended-updates>', () => {
  let app: ExtendedUpdatesAppElement;

  async function setupAppElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = new ExtendedUpdatesAppElement();
    document.body.appendChild(app);
    await flushTasks();
  }

  function getEnableButton(): HTMLElement|null {
    return app.shadowRoot!.querySelector<HTMLElement>('#enableButton');
  }

  function getCancelButton(): HTMLElement|null {
    return app.shadowRoot!.querySelector<HTMLElement>('#cancelButton');
  }

  function getPopupDialog(): CrDialogElement|null {
    return app.shadowRoot!.querySelector<CrDialogElement>('#popupDialog');
  }

  function getPopupConfirmButton(): HTMLElement|null {
    return app.shadowRoot!.querySelector<HTMLElement>('#popupConfirmButton');
  }

  function getPopupCancelButton(): HTMLElement|null {
    return app.shadowRoot!.querySelector<HTMLElement>('#popupCancelButton');
  }

  function assertPopupVisibility(expectedVisibility: boolean): void {
    const popup = getPopupDialog();
    const confirmButton = getPopupConfirmButton();
    const cancelButton = getPopupCancelButton();

    assertEquals(expectedVisibility, isVisible(popup ? popup.$.dialog : popup));
    assertEquals(expectedVisibility, isVisible(confirmButton));
    assertEquals(expectedVisibility, isVisible(cancelButton));
  }

  suite('<app-test>', () => {
    let browserProxy: TestExtendedUpdatesBrowserProxy;

    setup(async () => {
      browserProxy = new TestExtendedUpdatesBrowserProxy();
      ExtendedUpdatesBrowserProxy.setInstance(browserProxy);

      await setupAppElement();
    });

    test('opt in successfully', async () => {
      assertPopupVisibility(false);

      const enableButton = getEnableButton();
      assertTrue(!!enableButton);
      assertTrue(isVisible(enableButton));
      enableButton.click();
      await flushTasks();

      assertPopupVisibility(true);
      const popupConfirmButton = getPopupConfirmButton();
      assertTrue(!!popupConfirmButton);
      assertTrue(isVisible(popupConfirmButton));
      popupConfirmButton.click();
      await browserProxy.whenCalled('optInToExtendedUpdates');
      await browserProxy.whenCalled('closeDialog');
    });

    test('popup cancel button closes popup', async () => {
      assertPopupVisibility(false);

      const enableButton = getEnableButton();
      assertTrue(!!enableButton);
      assertTrue(isVisible(enableButton));
      enableButton.click();
      await flushTasks();

      assertPopupVisibility(true);
      const popupCancelButton = getPopupCancelButton();
      assertTrue(!!popupCancelButton);
      assertTrue(isVisible(popupCancelButton));
      popupCancelButton.click();
      await flushTasks();

      assertPopupVisibility(false);
      assertTrue(!!enableButton);
      assertTrue(isVisible(enableButton));
    });

    test('dialog cancel button closes dialog', async () => {
      const cancelButton = getCancelButton();
      assertTrue(!!cancelButton);
      assertTrue(isVisible(cancelButton));
      cancelButton.click();
      await browserProxy.whenCalled('closeDialog');
    });
  });

  suite('<util>', () => {
    setup(async () => {
      await setupAppElement();
    });

    test('perform opt in', async () => {
      const enableButton = getEnableButton();
      assertTrue(!!enableButton);
      enableButton.click();
      await flushTasks();

      const popupConfirmButton = getPopupConfirmButton();
      assertTrue(!!popupConfirmButton);
      popupConfirmButton.click();
      await flushTasks();
    });
  });
});
