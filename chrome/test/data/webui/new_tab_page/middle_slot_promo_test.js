// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, PromoBrowserCommandProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageMiddleSlotPromoTest', () => {
  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();
    testProxy = createTestProxy();
  });

  async function createMiddleSlotPromo() {
    testProxy.handler.setResultFor('getPromo', Promise.resolve({
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
    BrowserProxy.instance_ = testProxy;
    const loaded =
        eventToPromise('ntp-middle-slot-promo-loaded', document.body);
    const middleSlotPromo = document.createElement('ntp-middle-slot-promo');
    document.body.appendChild(middleSlotPromo);
    await loaded;
    return middleSlotPromo;
  }

  test('render', async () => {
    const middleSlotPromo = await createMiddleSlotPromo();
    const parts = middleSlotPromo.$.container.children;
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

  test('clicking on command', async () => {
    const middleSlotPromo = await createMiddleSlotPromo();
    const testProxy = PromoBrowserCommandProxy.getInstance();
    testProxy.handler = TestBrowserProxy.fromClass(
        promoBrowserCommand.mojom.CommandHandlerRemote);
    testProxy.handler.setResultFor('executeCommand', Promise.resolve());
    const imageWithCommand = middleSlotPromo.$.container.children[2];
    const command = middleSlotPromo.$.container.children[5];
    await Promise.all([imageWithCommand, command].map(async el => {
      testProxy.handler.reset();
      el.click();
      // Make sure the command and click information are sent to the browser.
      const [expectedCommand, expectedClickInfo] =
          await testProxy.handler.whenCalled('executeCommand');
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

  test('sends loaded event if no promo', async () => {
    testProxy.handler.setResultFor('getPromo', Promise.resolve({promo: null}));
    const loaded =
        eventToPromise('ntp-middle-slot-promo-loaded', document.body);
    const middleSlotPromo = document.createElement('ntp-middle-slot-promo');
    document.body.appendChild(middleSlotPromo);
    await loaded;
  });
});
