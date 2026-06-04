// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {aimBrowserProxyFactory, OmniboxPopupAimPageHandlerRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxAimAppElement, OmniboxPopupAimPageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

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
    isCanvasQuerySubmitted: false,
  };
}

suite('AimAppTest', function() {
  let handler: TestMock<OmniboxPopupAimPageHandlerRemote>&
      OmniboxPopupAimPageHandlerRemote;
  let page: OmniboxPopupAimPageRemote;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(OmniboxPopupAimPageHandlerRemote);
    const {instance, remote} = aimBrowserProxyFactory.createForTest(handler);
    aimBrowserProxyFactory.setInstance(instance);
    page = remote;
    metrics = fakeMetricsPrivate();
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceCobrowsingComposeboxEnabled: false,
      contextButtonShapeIsOblong: false,
      webuiOmniboxSimplificationEnabled: false,
      composeboxSmartTabSharingVisible: false,
      contextualMenuUsePecApi: false,
    });
  });

  teardown(() => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceCobrowsingComposeboxEnabled: false,
      contextualMenuUsePecApi: false,
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
    page.clearPopup();
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
    page.setPreserveContextOnClose(true);
    page.clearPopup();
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
    page.setPreserveContextOnClose(true);
    page.clearPopup();
    await microtasksFinished();

    // Re-open (onPopupShown) should reset preserveContextOnClose to false.
    page.onPopupShown({
      input: '',
      attachments: [],
      toolMode: 0,
    });
    await microtasksFinished();

    // Close again, should clear input because it was reset to false.
    page.clearPopup();
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

    page.onPopupShown({
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
    page.setPreserveContextOnClose(true);

    page.onPopupShown({
      input: '',
      attachments: [],
      toolMode: 0,
    });
    await microtasksFinished();
    // Should NOT have played.
    assertTrue(!glowAnimationPlayed);

    // Reset for next show (implicit in onPopupShown).
    // If we show again, it SHOULD play.
    page.onPopupShown({
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

    const result = await handler.whenCalled('showContextMenu');
    assertEquals(point.x, result.x);
    assertEquals(point.y, result.y);
  });

  test('ContextMenuEntrypointMenuOpenWorkaround', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);
    await microtasksFinished();

    // Enable the context menu so the real entrypoint button is rendered.
    app.$.composebox.contextMenuEnabled = true;
    await microtasksFinished();

    const contextButton = app.$.composebox.getContextEntrypointElement();
    assertTrue(!!contextButton);

    // Click event triggers workaround.
    app.$.composebox.dispatchEvent(
        new CustomEvent('context-menu-entrypoint-click', {
          detail: {x: 10, y: 20},
          bubbles: true,
          composed: true,
        }));

    assertTrue(contextButton.classList.contains('menu-open'));

    // Mojom callback clears class.
    page.onContextMenuClosed();
    await microtasksFinished();

    assertFalse(contextButton.classList.contains('menu-open'));
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

  test('adjusts size on voice permissions dialogue changed', async () => {
    const app: OmniboxAimAppElement = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);
    await microtasksFinished();

    // Simulate the event being fired with specific dimensions.
    app.$.composebox.dispatchEvent(
        new CustomEvent('embedded-voice-permission-prompt-changed', {
          detail: {
            isOpened: true,
            height: 120,
            width: 250,
          },
          bubbles: true,
          composed: true,
        }));

    await microtasksFinished();

    // Verify CSS custom properties are updated on composebox.
    assertTrue(
        app.$.composebox.classList.contains('has-embedded-permission-prompt'));
    assertEquals(
        '120px',
        app.$.composebox.style.getPropertyValue(
            '--cr_composebox_minimum_height'));
    assertEquals(
        '250px',
        app.$.composebox.style.getPropertyValue(
            '--cr_composebox_minimum_width'));

    // Simulate the dialogue closing.
    app.$.composebox.dispatchEvent(
        new CustomEvent('embedded-voice-permission-prompt-changed', {
          detail: {isOpened: false, height: 0, width: 0},
          bubbles: true,
          composed: true,
        }));

    await microtasksFinished();

    // Verify CSS custom properties are reset.
    assertFalse(
        app.$.composebox.classList.contains('has-embedded-permission-prompt'));
    assertEquals(
        '',
        app.$.composebox.style.getPropertyValue(
            '--cr_composebox_minimum_height'));
    assertEquals(
        '',
        app.$.composebox.style.getPropertyValue(
            '--cr_composebox_minimum_width'));
  });
});
