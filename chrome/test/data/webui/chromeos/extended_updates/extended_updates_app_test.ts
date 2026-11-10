// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extended-updates-dialog/app.js';

import {ExtendedUpdatesAppElement} from 'chrome://extended-updates-dialog/app.js';
import type {App} from 'chrome://extended-updates-dialog/extended_updates.mojom-webui.js';
import {browserProxyFactory, PageHandlerRemote} from 'chrome://extended-updates-dialog/extended_updates.mojom-webui.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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
    let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

    setup(async () => {
      handler = TestMock.fromClass(PageHandlerRemote);
      handler.setResultFor(
          'optInToExtendedUpdates', Promise.resolve({success: true}));
      handler.setResultFor(
          'getInstalledAndroidApps', Promise.resolve({apps: []}));
      const {instance} = browserProxyFactory.createForTest(handler);
      browserProxyFactory.setInstance(instance);

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
      await handler.whenCalled('optInToExtendedUpdates');
      await handler.whenCalled('closeDialog');
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
      await handler.whenCalled('closeDialog');
    });

    test('apps property reactive binding', async () => {
      const mockApps: App[] = [
        {id: '1', title: 'App 1'},
        {id: '2', title: 'App 2'},
      ];
      handler.setResultFor(
          'getInstalledAndroidApps', Promise.resolve({apps: mockApps}));

      await setupAppElement();

      const androidAppsList =
          app.shadowRoot!.querySelector('android-apps-list');
      assertTrue(!!androidAppsList);
      assertEquals(mockApps, (androidAppsList as any).apps);
    });

    test('showPopup property reactive binding', async () => {
      assertEquals(false, (app as any).showPopup);
      assertPopupVisibility(false);

      const enableButton = getEnableButton();
      assertTrue(!!enableButton);
      enableButton.click();
      await flushTasks();

      assertEquals(true, (app as any).showPopup);
      assertPopupVisibility(true);
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
