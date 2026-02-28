// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, PageCallbackRouter, PageHandlerRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {PageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

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
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestAimBrowserProxy();
    BrowserProxy.setInstance(testProxy as unknown as BrowserProxy);
    metrics = fakeMetricsPrivate();
  });

  // TODO(crbug.com/479888362): Disabled by gardener due to failure without
  // clear culprit.
  test.skip('ClearsInputOnCloseByDefault', async function() {
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

    // There's no search context being added when setting the input, therefore,
    // no context added histogram should get recorded.
    assertEquals(
        0,
        metrics.count(
            'ContextualSearch.ContextAdded.ContextAddedMethod.Omnibox'));
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
