// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import type {CrAutoImgElement, MobilePromoElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('MobilePromoTest', () => {
  let newTabPageHandler: TestMock<PageHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    newTabPageHandler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    void newTabPageHandler;
  });

  async function createMobilePromo(hasQrCode: boolean):
      Promise<MobilePromoElement> {
    if (hasQrCode) {
      newTabPageHandler.setResultFor(
          'getMobilePromoQrCode', Promise.resolve({qrCode: 'abc'}));
    } else {
      newTabPageHandler.setResultFor(
          'getMobilePromoQrCode', Promise.resolve({qrCode: ''}));
    }
    const mobilePromo = document.createElement('ntp-mobile-promo');
    document.body.appendChild(mobilePromo);

    await eventToPromise('qr-code-changed', mobilePromo);
    await microtasksFinished();

    return mobilePromo;
  }

  function assertHasQrCode(
      hasQrCode: boolean, mobilePromo: MobilePromoElement) {
    const image = $$<CrAutoImgElement>(mobilePromo, '#qrCode');
    assertTrue(!!image);
    assertEquals(hasQrCode, !image.hidden);
    if (hasQrCode) {
      assertTrue(isVisible(image));
      assertDeepEquals('data:image/webp;base64,abc', image.src);
    } else {
      assertFalse(isVisible(image));
      assertDeepEquals('data:image/webp;base64,', image.src);
    }
  }

  test('render hasQrCode=true', async () => {
    const hasQrCode = true;
    const mobilePromo = await createMobilePromo(hasQrCode);
    assertHasQrCode(hasQrCode, mobilePromo);
  });

  test('render hasQrCode=false', async () => {
    const hasQrCode = false;
    const mobilePromo = await createMobilePromo(hasQrCode);
    assertHasQrCode(hasQrCode, mobilePromo);
  });

  test('mobile promo dismisses when dismiss is clicked', async () => {
    newTabPageHandler.setResultFor(
        'getMobilePromoQrCode', Promise.resolve({qrCode: 'abc'}));
    const mobilePromo = await createMobilePromo(true);
    assertTrue(isVisible(mobilePromo.$.promoContainer));
    assertFalse(mobilePromo.$.dismissPromoButtonToast.open);

    const parts = mobilePromo.$.titleAndDismissContainer.children;
    assertEquals(2, parts.length);

    const dismissPromoButton = parts[1] as HTMLElement;
    dismissPromoButton.click();

    await microtasksFinished();

    assertFalse(isVisible(mobilePromo.$.promoContainer));
    assertTrue(mobilePromo.$.dismissPromoButtonToast.open);
  });

  test('mobile promo dismissed can undo', async () => {
    newTabPageHandler.setResultFor(
        'getMobilePromoQrCode', Promise.resolve({qrCode: 'abc'}));
    const mobilePromo = await (createMobilePromo(true));
    assertTrue(isVisible(mobilePromo.$.promoContainer));
    assertFalse(mobilePromo.$.dismissPromoButtonToast.open);

    const parts = mobilePromo.$.titleAndDismissContainer.children;
    assertEquals(2, parts.length);

    const dismissPromoButton = parts[1] as HTMLElement;
    dismissPromoButton.click();

    await microtasksFinished();

    assertFalse(isVisible(mobilePromo.$.promoContainer));
    assertTrue(mobilePromo.$.dismissPromoButtonToast.open);

    mobilePromo.$.undoDismissPromoButton.click();

    await microtasksFinished();

    assertTrue(isVisible(mobilePromo.$.promoContainer));
    assertFalse(mobilePromo.$.dismissPromoButtonToast.open);
  });
});
