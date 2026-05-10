// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, PageCallbackRouter, PageHandlerRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxAimAppElement, PageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

function createDefaultInputState(): InputState {
  return {
    allowedModels: [],
    allowedTools: [],
    allowedInputTypes: [],
    activeModel: 0,
    activeTool: 0,
    disabledModels: [],
    disabledTools: [],
    disabledInputTypes: [],
    toolConfigs: [],
    modelConfigs: [],
    inputTypeConfigs: [],
    toolsSectionConfig: null,
    modelSectionConfig: null,
    hintText: '',
    maxInputsByType: {},
    maxTotalInputs: 0,
  };
}

suite('AimAppTest', function() {
  let testProxy: TestAimBrowserProxy;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestAimBrowserProxy();
    BrowserProxy.setInstance(testProxy as unknown as BrowserProxy);
    metrics = fakeMetricsPrivate();
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceCobrowsingComposeboxEnabled: false,
    });
  });

  teardown(() => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceCobrowsingComposeboxEnabled: false,
    });
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
    assertTrue(!!app.$.composebox.input);

    // Close without preserving context (default is false).
    testProxy.page.clearPopup();
    await microtasksFinished();
    assertTrue(!app.$.composebox.input);
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
    assertTrue(!!app.$.composebox.input);

    // Close with preserving context.
    testProxy.page.setPreserveContextOnClose(true);
    testProxy.page.clearPopup();
    await microtasksFinished();
    assertTrue(!!app.$.composebox.input);
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
    testProxy.page.clearPopup();
    await microtasksFinished();

    // Re-open (onPopupShown) should reset preserveContextOnClose to false.
    testProxy.page.onPopupShown({
      input: '',
      attachments: [],
      toolMode: 0,
    });
    await microtasksFinished();

    // Close again, should clear input because it was reset to false.
    testProxy.page.clearPopup();
    await microtasksFinished();
    assertTrue(!app.$.composebox.input);

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

  test('ShowsContextMenuOnContextualEntryPointClick', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    const point = {x: 10, y: 20};
    app.$.composebox.dispatchEvent(
        new CustomEvent('context-menu-entrypoint-click', {
          detail: point,
          bubbles: true,
          composed: true,
        }));

    const result = await testProxy.handler.whenCalled('showContextMenu');
    assertEquals(point.x, result.x);
    assertEquals(point.y, result.y);
  });

  test('UsesCompactLayoutInTallModeWhenNoAllowedInputs', async function() {
    const app: OmniboxAimAppElement = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);
    await microtasksFinished();

    // Force a 'Tall' layout mode.
    app.setSearchboxLayoutModeForTesting('TallBottomContext');
    app.setHasAllowedInputsForTesting(false);

    assertEquals('Compact', app.getSearchboxLayoutModeForTesting());

    // Now simulate allowed inputs.
    app.$.composebox.dispatchEvent(new CustomEvent('input-state-changed', {
      detail: {
        inputState: {
          ...createDefaultInputState(),
          allowedModels: [1],
        },
      },
    }));

    await microtasksFinished();
    assertTrue(app.getHasAllowedInputsForTesting());
    assertEquals('TallBottomContext', app.getSearchboxLayoutModeForTesting());

    // Now simulate no allowed inputs again.
    app.$.composebox.dispatchEvent(new CustomEvent('input-state-changed', {
      detail: {
        inputState: createDefaultInputState(),
      },
    }));

    await microtasksFinished();
    assertTrue(!app.getHasAllowedInputsForTesting());
    assertEquals('Compact', app.getSearchboxLayoutModeForTesting());
  });

  test(
      'Voice search animation is not enabled if voice coherence is disabled',
      async function() {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });
        const app = document.createElement('omnibox-aim-app');
        document.body.appendChild(app);
        await microtasksFinished();

        // TODO(crbug.com/497887993) - replace with `ComposeboxElement` once
        // `ComposeboxElement` usage is unrestricted after the composebox
        // migration.
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const composebox = app.$.composebox as any;
        assertTrue(!!composebox, 'Composebox should exist');
        assertTrue(
            !!composebox.$.animatedSearchElement,
            'animation element should exist');
        assertFalse(
            composebox.$.animatedSearchElement.requiresVoice,
            'voice search animation should not exist');
      });

  test(
      'Voice search animation is disabled ' +
          'if only cobrowsing voice coherence is enabled',
      async function() {
        // Only enabled in cobrowsing means not enabled in
        // omnibox.
        loadTimeData.overrideValues({
          // If composebox cobrowsing is enabled, backend logic
          // should calculate `voiceSearchCoherenceComposeboxesEnabled`
          // as false. Mock it as false here, so check that
          // the frontend only depends on
          // `voiceSearchCoherenceComposeboxesEnabled` and not
          // `voiceSearchCoherenceCobrowsingComposeboxEnabled`.
          voiceSearchCoherenceComposeboxesEnabled: false,
          voiceSearchCoherenceCobrowsingComposeboxEnabled: true,
        });
        const app = document.createElement('omnibox-aim-app');
        document.body.appendChild(app);
        await microtasksFinished();

        // TODO(crbug.com/497887993) - replace with `ComposeboxElement` once
        // `ComposeboxElement` usage is unrestricted after the composebox
        // migration.
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const composebox = app.$.composebox as any;
        assertTrue(!!composebox, 'Composebox should exist');
        assertTrue(
            !!composebox.$.animatedSearchElement,
            'animation element should exist');
        assertFalse(
            composebox.$.animatedSearchElement.requiresVoice,
            'voice search animation should not exist');
      });

  test(
      'Voice search animation is enabled if voice coherence is enabled',
      async function() {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
          voiceSearchCoherenceCobrowsingComposeboxEnabled: false,
        });

        const app = document.createElement('omnibox-aim-app');
        document.body.appendChild(app);
        await microtasksFinished();

        // TODO(crbug.com/497887993) - replace with `ComposeboxElement` once
        // `ComposeboxElement` usage is unrestricted after the composebox
        // migration.
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const composebox = app.$.composebox as any;
        assertTrue(!!composebox, 'Composebox should exist');
        assertTrue(
            !!composebox.$.animatedSearchElement,
            'animation element should exist');
        assertTrue(
            composebox.$.animatedSearchElement.requiresVoice,
            'voice search animation should exist');
      });
});
