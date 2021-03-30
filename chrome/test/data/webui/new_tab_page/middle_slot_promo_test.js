// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';
import {$$, NewTabPageProxy, PromoBrowserCommandProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageMiddleSlotPromoTest', () => {
  /**
   * @implements {newTabPage.mojom.PageHandlerRemote}
   * @extends {TestBrowserProxy}
   */
  let newTabPageHandler;

  /**
   * @implements {promoBrowserCommand.mojom.CommandHandlerRemote}
   * @extends {TestBrowserProxy}
   */
  let promoBrowserCommandHandler;

  setup(() => {
    PolymerTest.clearBody();
    newTabPageHandler =
        TestBrowserProxy.fromClass(newTabPage.mojom.PageHandlerRemote);
    NewTabPageProxy.setInstance(
        newTabPageHandler, new newTabPage.mojom.PageCallbackRouter());

    promoBrowserCommandHandler = TestBrowserProxy.fromClass(
        promoBrowserCommand.mojom.CommandHandlerRemote);
    const promoBrowserCommandTestProxy = PromoBrowserCommandProxy.getInstance();
    promoBrowserCommandTestProxy.handler = promoBrowserCommandHandler;
  });

  /**
   * @param {boolean} canShowPromo
   * @return {!Element}
   */
  async function createMiddleSlotPromo(canShowPromo) {
    newTabPageHandler.setResultFor('getPromo', Promise.resolve({
      promo: {
        middleSlotParts: [
          {image: {imageUrl: {url: 'https://image'}}},
          {
            image: {
              imageUrl: {url: 'https://image'},
              target: {url: 'https://link'},
            }
          },
          {
            image: {
              imageUrl: {url: 'https://image'},
              target: {url: 'command:123'},
            }
          },
          {text: {text: 'text', color: 'red'}},
          {
            link: {
              url: {url: 'https://link'},
              text: 'link',
              color: 'green',
            }
          },
          {
            link: {
              url: {url: 'command:123'},
              text: 'command',
              color: 'blue',
            }
          },
        ],
      },
    }));

    promoBrowserCommandHandler.setResultFor(
        'canShowPromoWithCommand', Promise.resolve({canShow: canShowPromo}));

    const middleSlotPromo = document.createElement('ntp-middle-slot-promo');
    document.body.appendChild(middleSlotPromo);
    const loaded =
        eventToPromise('ntp-middle-slot-promo-loaded', document.body);
    await promoBrowserCommandHandler.whenCalled('canShowPromoWithCommand');
    assertEquals(
        2, promoBrowserCommandHandler.getCallCount('canShowPromoWithCommand'));
    if (canShowPromo) {
      await newTabPageHandler.whenCalled('onPromoRendered');
    } else {
      assertEquals(0, newTabPageHandler.getCallCount('onPromoRendered'));
    }
    await loaded;
    return middleSlotPromo;
  }

  /**
   * @param {boolean} hasContent
   * @param {!Element} middleSlotPromo
   * @private
   */
  function assertHasContent(hasContent, middleSlotPromo) {
    assertEquals(hasContent, !!$$(middleSlotPromo, '#container'));
  }

  test(`render canShowPromo=true`, async () => {
    const canShowPromo = true;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
    const parts = $$(middleSlotPromo, '#container').children;
    assertEquals(6, parts.length);
    const [image, imageWithLink, imageWithCommand, text, link, command] = parts;

    assertEquals('https://image', image.autoSrc);

    assertEquals('https://link/', imageWithLink.href);
    assertEquals('https://image', imageWithLink.children[0].autoSrc);

    assertEquals('', imageWithCommand.href);
    assertEquals('https://image', imageWithCommand.children[0].autoSrc);

    assertEquals('text', text.innerText);
    assertEquals('red', text.style.color);

    assertEquals('https://link/', link.href);
    assertEquals('link', link.innerText);
    assertEquals('green', link.style.color);

    assertEquals('', command.href);
    assertEquals('command', command.text);
    assertEquals('blue', command.style.color);
  });

  test(`render canShowPromo=false`, async () => {
    const canShowPromo = false;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
  });

  test('clicking on command', async () => {
    const canShowPromo = true;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
    promoBrowserCommandHandler.setResultFor(
        'executeCommand', Promise.resolve());
    const imageWithCommand = $$(middleSlotPromo, '#container').children[2];
    const command = $$(middleSlotPromo, '#container').children[5];
    await Promise.all([imageWithCommand, command].map(async el => {
      promoBrowserCommandHandler.reset();
      el.click();
      // Make sure the command and click information are sent to the browser.
      const [expectedCommand, expectedClickInfo] =
          await promoBrowserCommandHandler.whenCalled('executeCommand');
      // Unsupported commands get resolved to the default command before being
      // sent to the browser.
      assertEquals(
          promoBrowserCommand.mojom.Command.kUnknownCommand, expectedCommand);
      assertDeepEquals(
          {
            middleButton: false,
            altKey: false,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
          expectedClickInfo);
    }));
  });

  [null,
   {middleSlotParts: []},
   {middleSlotParts: [{break: {}}]},
  ].forEach((promo, i) => {
    test(`promo remains hidden if there is no data ${i}`, async () => {
      newTabPageHandler.setResultFor('getPromo', Promise.resolve({promo}));
      const middleSlotPromo = document.createElement('ntp-middle-slot-promo');
      document.body.appendChild(middleSlotPromo);
      await flushTasks();
      assertEquals(
          0,
          promoBrowserCommandHandler.getCallCount('canShowPromoWithCommand'));
      assertEquals(0, newTabPageHandler.getCallCount('onPromoRendered'));
      assertHasContent(false, middleSlotPromo);
    });
  });
});
