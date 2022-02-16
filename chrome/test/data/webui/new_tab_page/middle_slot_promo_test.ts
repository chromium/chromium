// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/lazy_load.js';

import {MiddleSlotPromoElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$, BrowserCommandProxy, CrAutoImgElement, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {Command, CommandHandlerRemote} from 'chrome://resources/js/browser_command/browser_command.mojom-webui.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, flushTasks} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('NewTabPageMiddleSlotPromoTest', () => {
  let newTabPageHandler: TestBrowserProxy;
  let promoBrowserCommandHandler: TestBrowserProxy;

  setup(() => {
    document.body.innerHTML = '';
    newTabPageHandler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));

    promoBrowserCommandHandler = installMock(
        CommandHandlerRemote,
        mock => BrowserCommandProxy.setInstance({handler: mock}));
  });

  async function createMiddleSlotPromo(canShowPromo: boolean):
      Promise<MiddleSlotPromoElement> {
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
        'canExecuteCommand', Promise.resolve({canExecute: canShowPromo}));

    const middleSlotPromo = document.createElement('ntp-middle-slot-promo');
    document.body.appendChild(middleSlotPromo);
    const loaded =
        eventToPromise('ntp-middle-slot-promo-loaded', document.body);
    await promoBrowserCommandHandler.whenCalled('canExecuteCommand');
    assertEquals(
        2, promoBrowserCommandHandler.getCallCount('canExecuteCommand'));
    if (canShowPromo) {
      await newTabPageHandler.whenCalled('onPromoRendered');
    } else {
      assertEquals(0, newTabPageHandler.getCallCount('onPromoRendered'));
    }
    await loaded;
    return middleSlotPromo;
  }

  function assertHasContent(
      hasContent: boolean, middleSlotPromo: MiddleSlotPromoElement) {
    assertEquals(hasContent, !!$$(middleSlotPromo, '#container'));
  }

  test(`render canShowPromo=true`, async () => {
    const canShowPromo = true;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
    const parts = $$(middleSlotPromo, '#container')!.children;
    assertEquals(6, parts.length);

    const image = parts[0] as CrAutoImgElement;
    const imageWithLink = parts[1] as HTMLAnchorElement;
    const imageWithCommand = parts[2] as HTMLAnchorElement;
    const text = parts[3] as HTMLElement;
    const link = parts[4] as HTMLAnchorElement;
    const command = parts[5] as HTMLAnchorElement;

    assertEquals('https://image', image.autoSrc);

    assertEquals('https://link/', imageWithLink.href);
    assertEquals(
        'https://image',
        (imageWithLink.children[0] as CrAutoImgElement).autoSrc);

    assertEquals('', imageWithCommand.href);
    assertEquals(
        'https://image',
        (imageWithCommand.children[0] as CrAutoImgElement).autoSrc);

    assertEquals('text', text.innerText);
    assertEquals('red', text.style.color);

    assertEquals('https://link/', link.href);
    assertEquals('link', link.innerText);
    assertEquals('green', link.style.color);

    assertEquals('', command.href);
    assertEquals('command', command.text);
    assertEquals('blue', command.style.color);
  });

  test('render canShowPromo=false', async () => {
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
    const imageWithCommand =
        $$(middleSlotPromo, '#container')!.children[2] as HTMLElement;
    const command =
        $$(middleSlotPromo, '#container')!.children[5] as HTMLElement;

    async function testClick(el: HTMLElement) {
      promoBrowserCommandHandler.reset();
      el.click();
      // Make sure the command and click information are sent to the browser.
      const [expectedCommand, expectedClickInfo] =
          await promoBrowserCommandHandler.whenCalled('executeCommand');
      // Unsupported commands get resolved to the default command before being
      // sent to the browser.
      assertEquals(Command.kUnknownCommand, expectedCommand);
      assertDeepEquals(
          {
            middleButton: false,
            altKey: false,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
          expectedClickInfo);
    }

    await testClick(imageWithCommand);
    await testClick(command);
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
          0, promoBrowserCommandHandler.getCallCount('canExecuteCommand'));
      assertEquals(0, newTabPageHandler.getCallCount('onPromoRendered'));
      assertHasContent(false, middleSlotPromo);
    });
  });
});
