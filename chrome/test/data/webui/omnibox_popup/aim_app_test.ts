// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, PageCallbackRouter, PageHandlerRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {PageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

class TestAimBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  page: PageRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

suite('AimAppTest', function() {
  let testProxy: TestAimBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestAimBrowserProxy();
    BrowserProxy.setInstance(testProxy as unknown as BrowserProxy);
  });

  test('ContextMenuPrevented', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);
    const whenFired = eventToPromise('contextmenu', document.documentElement);
    document.documentElement.dispatchEvent(
        new Event('contextmenu', {cancelable: true}));
    const e = await whenFired;
    assertTrue(e.defaultPrevented);
  });

  test('ClearsInputOnCloseByDefault', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    // Set some input.
    app.$.composebox.addSearchContext({
      input: 'test input',
      attachments: [],
      toolMode: 0,
    });
    assertTrue(!!app.$.composebox.getInputText());

    // Close without preserving context (default is false).
    testProxy.page.onPopupHidden();
    await microtasksFinished();
    assertTrue(!app.$.composebox.getInputText());
  });

  test('PreservesInputOnCloseWhenRequested', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    // Set some input.
    app.$.composebox.addSearchContext({
      input: 'test input',
      attachments: [],
      toolMode: 0,
    });
    assertTrue(!!app.$.composebox.getInputText());

    // Close with preserving context.
    testProxy.page.setPreserveContextOnClose(true);
    testProxy.page.onPopupHidden();
    await microtasksFinished();
    assertTrue(!!app.$.composebox.getInputText());
  });

  test('ResetsPreserveContextOnShow', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    // Set some input.
    app.$.composebox.addSearchContext({
      input: 'test input',
      attachments: [],
      toolMode: 0,
    });

    // Close with preserving context.
    testProxy.page.setPreserveContextOnClose(true);
    testProxy.page.onPopupHidden();
    await microtasksFinished();

    // Re-open (onPopupShown) should reset preserveContextOnClose to false.
    testProxy.page.onPopupShown({
      input: '',
      attachments: [],
      toolMode: 0,
    });
    await microtasksFinished();

    // Close again, should clear input because it was reset to false.
    testProxy.page.onPopupHidden();
    await microtasksFinished();
    assertTrue(!app.$.composebox.getInputText());
  });

  test('PlaysGlowAnimationOnShowByDefault', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    let glowAnimationPlayed = false;
    app.$.composebox.playGlowAnimation = () => {
      glowAnimationPlayed = true;
    };

    testProxy.page.onPopupShown({
      input: '',
      attachments: [],
      toolMode: 0,
    });
    await microtasksFinished();
    assertTrue(glowAnimationPlayed);
  });

  test('SkipsGlowAnimationWhenPreservingContext', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    let glowAnimationPlayed = false;
    app.$.composebox.playGlowAnimation = () => {
      glowAnimationPlayed = true;
    };

    // Simulate preserving context.
    testProxy.page.setPreserveContextOnClose(true);

    testProxy.page.onPopupShown({
      input: '',
      attachments: [],
      toolMode: 0,
    });
    await microtasksFinished();
    // Should NOT have played.
    assertTrue(!glowAnimationPlayed);

    // Reset for next show (implicit in onPopupShown).
    // If we show again, it SHOULD play.
    testProxy.page.onPopupShown({
      input: '',
      attachments: [],
      toolMode: 0,
    });
    await microtasksFinished();
    assertTrue(glowAnimationPlayed);
  });
});
