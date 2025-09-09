// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextMenuEntrypointElement} from 'chrome://resources/cr_components/composebox/context_menu_entrypoint.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  test('clicking entrypoint shows context menu', async () => {
    handler.setResultFor('getTabs', Promise.resolve([]));

    // Act.
    entrypoint.$.entrypoint.click();
    await microtasksFinished();

    // Assert.
    assertTrue(entrypoint.$.menu.open);
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
