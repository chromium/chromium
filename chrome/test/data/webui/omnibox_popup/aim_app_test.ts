// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {aimBrowserProxyFactory, ComposeboxProxyImpl, OmniboxPopupAimPageHandlerRemote, SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxAimAppElement, OmniboxPopupAimPageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDefaultInputState, TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('AimAppTest', function() {
  let handler: TestMock<OmniboxPopupAimPageHandlerRemote>&
      OmniboxPopupAimPageHandlerRemote;
  let page: OmniboxPopupAimPageRemote;
  let testProxy: TestSearchboxBrowserProxy;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    const mockComposeboxHandler =
        TestMock.fromClass(ComposeboxPageHandlerRemote);
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxHandler,
        testProxy.handler as unknown as SearchboxPageHandlerRemote,
        testProxy.callbackRouter as unknown as SearchboxPageCallbackRouter));
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
      webuiOmniboxFullPopupEnabled: false,
      composeboxSmartTabSharingVisible: false,
      contextManagementInComposeboxEnabled: false,
      contextualMenuUsePecApi: false,
    });
  });

  teardown(() => {
    document.body.style.width = '';
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

  test('ResetsPreserveContextOnAddContext', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    // Set preserve context on close.
    page.setPreserveContextOnClose(true);
    await microtasksFinished();

    // Adding context should reset `preserveContextOnClose` to false.
    page.addContext({
      input: 'test context',
      attachments: [],
      toolMode: 0,
    });
    await microtasksFinished();

    // Clear popup should clear inputs because `preserveContextOnClose` was
    // reset to false.
    page.clearPopup();
    await microtasksFinished();
    assertTrue(!app.$.composebox.input);
  });

  test('ClearPopupResetsSmartTabSharingUnlessPreserved', async function() {
    testProxy.handler.setPromiseResolveFor<'getSmartTabSharingActive'>(
        'getSmartTabSharingActive', {active: true});
    testProxy.handler.setPromiseResolveFor<'resetSmartTabSharing'>(
        'resetSmartTabSharing', {active: false});

    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);
    app.$.composebox.smartTabSharingVisible = true;
    await testProxy.handler.whenCalled('getSmartTabSharingActive');
    await microtasksFinished();
    assertTrue(app.$.composebox.smartTabSharingActive);

    // When preserveContextOnClose is true, clearPopup does not reset STS.
    page.setPreserveContextOnClose(true);
    await microtasksFinished();
    page.clearPopup();
    await microtasksFinished();
    assertEquals(0, testProxy.handler.getCallCount('resetSmartTabSharing'));
    assertTrue(app.$.composebox.smartTabSharingActive);

    // When preserveContextOnClose is false, clearPopup resets STS.
    page.setPreserveContextOnClose(false);
    await microtasksFinished();
    page.clearPopup();
    await testProxy.handler.whenCalled('resetSmartTabSharing');
    await microtasksFinished();
    assertEquals(1, testProxy.handler.getCallCount('resetSmartTabSharing'));
    assertFalse(app.$.composebox.smartTabSharingActive);
  });

  // Regression test for b/558982300: `AddContext` can be delivered while the
  // popup document is hidden (e.g. still occluded by a file picker). It must
  // not throw and must still reset `preserveContextOnClose`.
  test('AddContextWhileHiddenResetsPreserveContext', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    page.setPreserveContextOnClose(true);
    await microtasksFinished();

    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', configurable: true});
    try {
      page.addContext({
        input: 'test context',
        attachments: [],
        toolMode: 0,
      });
      await microtasksFinished();
    } finally {
      // Remove the own-property override to restore the prototype getter.
      delete (document as {visibilityState?: DocumentVisibilityState})
          .visibilityState;
    }

    page.clearPopup();
    await microtasksFinished();
    assertTrue(!app.$.composebox.input);
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
    document.body.style.width = '600px';
    document.body.appendChild(app);
    await microtasksFinished();

    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const composebox = app.$.composebox as any;
    composebox.showVoiceSearch = true;
    composebox.inVoiceSearchMode = true;
    await composebox.updateComplete;

    const voiceSearch = composebox.$.voiceSearch;
    assertTrue(!!voiceSearch);
    const voiceSearchContainer =
        voiceSearch.shadowRoot.querySelector('#container');
    assertTrue(!!voiceSearchContainer);

    // Ensure composebox maintains full container width in voice search mode
    // instead of falling back to the 337px default width.
    assertEquals(
        600, Math.round(app.$.composebox.getBoundingClientRect().width));

    // Simulate the event being fired from voice search with specific
    // dimensions larger than the default 121px voice search height.
    voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
      detail: {
        isOpened: true,
        height: 220,
        width: 250,
      },
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();
    await composebox.updateComplete;
    await voiceSearch.updateComplete;

    // Verify CSS custom properties and computed min dimensions are updated on
    // composebox and the voice search overlay container.
    assertTrue(app.$.composebox.classList.contains('has-permission-prompt'));
    assertEquals(
        '220px',
        app.$.composebox.style.getPropertyValue(
            '--cr_composebox_minimum_height'));
    assertEquals(
        '250px',
        app.$.composebox.style.getPropertyValue(
            '--cr_composebox_minimum_width'));
    assertEquals('220px', window.getComputedStyle(voiceSearch).minHeight);
    assertEquals('250px', window.getComputedStyle(voiceSearch).minWidth);
    assertEquals(
        '220px', window.getComputedStyle(voiceSearchContainer).minHeight);
    assertEquals(
        220, Math.round(app.$.composebox.getBoundingClientRect().height));
    assertEquals(
        220, Math.round(voiceSearchContainer.getBoundingClientRect().height));

    // Simulate the dialogue closing.
    voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
      detail: {isOpened: false, height: 0, width: 0},
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();
    await composebox.updateComplete;
    await voiceSearch.updateComplete;

    // Verify CSS custom properties are reset while full width is preserved.
    assertFalse(app.$.composebox.classList.contains('has-permission-prompt'));
    assertEquals(
        '',
        app.$.composebox.style.getPropertyValue(
            '--cr_composebox_minimum_height'));
    assertEquals(
        '',
        app.$.composebox.style.getPropertyValue(
            '--cr_composebox_minimum_width'));
    assertEquals(
        600, Math.round(app.$.composebox.getBoundingClientRect().width));
    document.body.style.width = '';
  });

  test(
      'Passes props and attributes to OmniboxComposeboxElement',
      async function() {
        loadTimeData.overrideValues({
          composeboxSmartComposeEnabled: true,
          caretAnimationEnabled: false,
          contextualMenuUsePecApi: true,
          searchboxLayoutMode: 'TallBottomContext',
          contextButtonShapeIsOblong: true,
          webuiOmniboxSimplificationEnabled: true,
          webuiOmniboxFullPopupEnabled: true,
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        const app = document.createElement('omnibox-aim-app');
        document.body.appendChild(app);
        await microtasksFinished();
        app.setHasAllowedInputsForTesting(true);
        await app.updateComplete;

        const composebox = app.$.composebox;

        assertTrue(composebox.hasAttribute('searchbox-next-enabled'));
        assertTrue(composebox.hasAttribute('disable-caret-color-animation'));
        assertEquals(
            'TallBottomContext',
            composebox.getAttribute('searchbox-layout-mode'));
        assertEquals('forward', composebox.submitButtonIconType);
        assertTrue(composebox.isOblongShape);
        assertTrue(composebox.webuiOmniboxSimplificationEnabled);
        assertTrue(composebox.closeOnEscape);
        assertTrue(composebox.showVoiceSearch);
        assertFalse(composebox.disableVoiceSearchAnimation);
        assertFalse(composebox.showMenuOnClick);
        assertTrue(composebox.shouldShowGhostFiles);
        assertTrue(composebox.usePecApi);
        assertTrue(composebox.smartComposeEnabled);
        assertTrue(composebox.composeboxNoFlickerSuggestionsFix);
      });

  test('ResetsSubmittingOnClose', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);

    app.$.composebox.submitting = true;

    page.clearPopup();
    await microtasksFinished();

    assertFalse(app.$.composebox.submitting);
  });

  test('PassesInputToRequestCloseOnCloseComposebox', async function() {
    const app = document.createElement('omnibox-aim-app');
    document.body.appendChild(app);
    await microtasksFinished();

    app.$.composebox.input = 'test draft text';
    app.$.composebox.dispatchEvent(new CustomEvent('close-composebox', {
      bubbles: true,
      composed: true,
      detail: {composeboxText: 'test draft text'},
    }));
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('requestClose'));
    assertEquals('test draft text', handler.getArgs('requestClose')[0]);
  });
});

