// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextMenuEntrypointElement} from 'chrome://resources/cr_components/composebox/context_menu_entrypoint.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

type Constructor<T> = new (...args: any[]) => T;
type Installer<T> = (instance: T) => void;

export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);
  installer(mock);
  return mock;
}

suite('ContextMenuEntrypoint', () => {
  let entrypoint: ContextMenuEntrypointElement;
  let handler: TestMock<PageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));

    entrypoint = new ContextMenuEntrypointElement();
    document.body.appendChild(entrypoint);
    await microtasksFinished();
  });

  test('menu is hidden initially', async () => {
    await microtasksFinished();
    assertFalse(entrypoint.$.menu.open);
  });

  test('clicking entrypoint shows context menu', async () => {
    handler.setResultFor('getTabs', Promise.resolve([]));

    // Act.
    entrypoint.$.entrypoint.click();
    await microtasksFinished();

    // Assert.
    assertTrue(entrypoint.$.menu.open);
  });

  test(
      'tab header is not displayed when there are no tab suggestions',
      async () => {
        // Arrange & Act.
        handler.setResultFor('getTabs', Promise.resolve([]));
        entrypoint.$.entrypoint.click();
        await microtasksFinished();
        assertTrue(entrypoint.$.menu.open);

        // Assert.
        const tabHeader = $$(entrypoint, '#tabHeader');
        assertFalse(!!tabHeader);
        const items = entrypoint.$.menu.querySelectorAll('.dropdown-item');
        assertEquals(2, items.length);
        assertEquals('imageUpload', items[0]!.id);
        assertEquals('fileUpload', items[1]!.id);
      });

  test(
      'clicking entrypoint shows context menu with correct items', async () => {
        // Arrange.
        handler.setResultFor('getTabs', Promise.resolve({
          tabs: [
            {
              title: 'Tab 1',
              url: {url: 'https://www.google.com'},
              tabId: 1,
            },
            {
              title: 'Tab 2',
              url: {url: 'https://www.google.com'},
              tabId: 2,
            },
          ],
        }));
        entrypoint.$.entrypoint.click();
        await microtasksFinished();
        assertTrue(entrypoint.$.menu.open);

        // Act & Assert.
        const tabHeader = $$(entrypoint, '#tabHeader');
        assertTrue(!!tabHeader);
        const items = entrypoint.$.menu.querySelectorAll('.dropdown-item');
        assertEquals(4, items.length);
        assertEquals('Tab 1', items[0]!.getAttribute('title'));
        assertEquals('Tab 2', items[1]!.getAttribute('title'));
        assertEquals('imageUpload', items[2]!.id);
        assertEquals('fileUpload', items[3]!.id);
      });

  ([
    ['#fileUpload', 'open-file-upload'],
    ['#imageUpload', 'open-image-upload'],
  ] as Array<[string, string]>)
      .forEach(([selector, eventName]) => {
        test(
            `clicking ${selector} propagates ${eventName} before closing menu`,
            async () => {
              // Arrange.
              handler.setResultFor('getTabs', Promise.resolve([]));
              entrypoint.$.entrypoint.click();
              await microtasksFinished();
              assertTrue(entrypoint.$.menu.open);

              // Act.
              const eventFired = eventToPromise(eventName, entrypoint);
              const button = $$(entrypoint, selector);
              assertTrue(!!button);
              button.click();
              await eventFired;

              // Assert.
              assertTrue(!!eventFired);

              assertFalse(entrypoint.$.menu.open);
            });
      });
});
