// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox.js';

import {ComposeboxFile, TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {ContextualEntrypointButtonElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

import {installMock} from './composebox_test_utils.js';

interface TestComposeboxElement extends ComposeboxElement {
  keepMenuOpenForMultiSelection: () => Promise<void>;
  keepMenuOpenOnTabSelectForRealbox: boolean;
  composeboxSource: string;
}

suite('ComposeboxTest', () => {
  let composebox: ComposeboxElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let searchboxHandler: SearchboxPageHandlerRemote&TestMock<SearchboxPageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.resetForTesting({
      composeboxShowImageSuggest: false,
      composeboxSmartComposeEnabled: false,
      composeboxShowContextMenuDescription: false,
      composeboxShowZps: true,
      composeboxContextDragAndDropEnabled: false,
      composeboxSource: 'NTP',
      composeboxFileMaxCount: 1,
      composeboxFileMaxSize: 1024,
      composeboxAttachmentFileTypes: '.pdf',
      composeboxImageFileTypes: 'image/png',
      lensSendRawFileMediaTypesEnabled: false,
      voiceSearchCoherenceAnySearchboxExperimentEnabled: false,
      voiceSearchCoherenceSearchboxEnabled: false,
      voiceSearchCoherenceComposeboxesEnabled: false,
      composeDeepSearchPlaceholder: 'Deep Search',
      composeCreateImagePlaceholder: 'Create Image',
      searchboxComposePlaceholder: 'Compose',
      composeboxShowContextMenu: false,
      composeboxShowTypedSuggest: false,
      composeboxCancelButtonTitleInput: 'Cancel input',
      composeboxCancelButtonTitle: 'Cancel',
      voiceSearchButtonLabel: 'Voice search',
      lensSearchButtonLabel: 'Lens search',
      lensSearchHint: 'Lens search',
      suggestionActivityLink: '<a>Activity</a>',
      composeboxSubmitButtonTitle: 'Submit',
      composeboxSmartComposeTabTitle: 'Tab',
      composeboxSmartComposeTitle: 'Smart Compose',
      voiceListening: 'Listening',
      voiceDetails: 'Details',
      voiceClose: 'Close',
      voiceStop: 'Stop',
      dismissButton: 'Dismiss',
      composeboxDragAndDropHint: 'Hint',
      removeSuggestion: 'Remove',
      composeboxDeleteFileTitle: 'Delete',
      tabFaviconChipsToCoinsEnabled: false,
      maxFilesReachedError: 'Max files reached',
      composeboxFileUploadInvalidTooLarge: 'File too large',
      composeboxFileUploadStartedText: 'Upload started',
    });

    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    handler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));

    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));

    searchboxCallbackRouterRemote =
        ComposeboxProxyImpl.getInstance()
            .searchboxCallbackRouter.$.bindNewPipeAndPassRemote();

    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await composebox.updateComplete;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('suggestion activity link triggers navigation', async () => {
    composebox.suggestionActivityEnabled = true;

    // Mock results to show the link.
    const matches = [
      createSearchMatchForTesting({
        isNoncannedAimSuggestion: true,
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const suggestionActivity = composebox.shadowRoot.querySelector('#suggestionActivity');
    assertTrue(!!suggestionActivity);
    const localizedLink = suggestionActivity.querySelector('localized-link');
    assertTrue(!!localizedLink);

    const testUrl = 'about:blank?activity';
    // Simulate the event fired by localized-link.
    const anchor = document.createElement('a');
    anchor.href = testUrl;

    let preventDefaultCalled = false;
    const linkClickedEvent = new CustomEvent('link-clicked', {
      detail: {
        event: {
          preventDefault: () => {
            preventDefaultCalled = true;
          },
          currentTarget: anchor,
        },
      },
    });

    localizedLink.dispatchEvent(linkClickedEvent);

    const url = await handler.whenCalled('navigateUrl');
    assertEquals(testUrl, url);
    assertTrue(preventDefaultCalled);
  });

  test('suggestion activity link hidden when suggestions are non canned', async () => {
    composebox.suggestionActivityEnabled = true;

    const matches = [
      createSearchMatchForTesting({
        isNoncannedAimSuggestion: false,
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const suggestionActivity = composebox.shadowRoot.querySelector('#suggestionActivity');
    assertFalse(!!suggestionActivity);
  });

  test(
      'smartComposeEnabled forwards from <cr-composebox> to <cr-composebox-input>',
      async () => {
        loadTimeData.overrideValues({composeboxSmartComposeEnabled: true});

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        const fresh = document.createElement('cr-composebox');
        document.body.appendChild(fresh);
        await fresh.updateComplete;

        const input = fresh.shadowRoot.querySelector<ComposeboxInputElement>(
            'cr-composebox-input');
        assertTrue(!!input);
        await input.updateComplete;

        assertTrue(input.hasAttribute('smart-compose-enabled'));
      });

  test(
      'voice permission changed updates search-animated-glow' +
          'class and hides audio-wave',
      async () => {
        // Mock WindowProxy to enable voice search.
        const windowProxy = installMock(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);

        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });

        // Recreate composebox so updated loadTimeData and WindowProxy mock take
        // effect.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composebox = document.createElement('cr-composebox');
        composebox.showVoiceSearch = true;
        document.body.appendChild(composebox);
        await composebox.updateComplete;

        const glow =
            composebox.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!glow);

        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        glow.shadowRoot.appendChild(style);

        // Make sure it is listening so the audio element becomes visible
        // (opacity 1).
        composebox.isListening = true;
        await composebox.updateComplete;
        await glow.updateComplete;

        assertTrue(glow.isListening, 'glow.isListening should be true');
        assertTrue(
            glow.hasAttribute('is-listening'),
            'glow should have is-listening attribute');

        const audioWave = glow.shadowRoot.querySelector('audio-wave');
        assertTrue(!!audioWave);
        assertEquals('1', window.getComputedStyle(audioWave).opacity);

        // Simulate voice permission prompt opening.
        composebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await composebox.updateComplete;

        // Verify the class was added and opacity turned to 0.
        assertTrue(glow.classList.contains('permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(audioWave).opacity);
      });

  test(
      'voice permission changed updates search-animated-glow class' +
          'and hides recording-wave',
      async () => {
        // Mock WindowProxy to enable voice search.
        const windowProxy = installMock(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);

        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        // Recreate composebox so updated loadTimeData and WindowProxy mock take
        // effect.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composebox = document.createElement('cr-composebox');
        composebox.showVoiceSearch = true;
        document.body.appendChild(composebox);
        await composebox.updateComplete;

        const glow =
            composebox.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!glow);

        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        glow.shadowRoot.appendChild(style);

        // Make sure it is listening so the audio element becomes visible
        // (opacity 1)
        composebox.isListening = true;
        await composebox.updateComplete;
        await glow.updateComplete;

        assertTrue(glow.isListening, 'glow.isListening should be true');
        assertTrue(
            glow.hasAttribute('is-listening'),
            'glow should have is-listening attribute');

        const recordingWave = glow.shadowRoot.querySelector('recording-wave');
        assertTrue(!!recordingWave);
        assertEquals('1', window.getComputedStyle(recordingWave).opacity);

        // Simulate voice permission prompt opening.
        composebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await composebox.updateComplete;

        // Verify the class was added and opacity turned to 0.
        assertTrue(glow.classList.contains('permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(recordingWave).opacity);
      });

  test(
      'voice permission changed updates cr-composebox-voice-search class' +
          'and hides bottomActions',
      async () => {
        // Mock WindowProxy to enable voice search.
        const windowProxy = installMock(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);

        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        // Recreate composebox so updated loadTimeData and WindowProxy mock take
        // effect.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composebox = document.createElement('cr-composebox');
        composebox.showVoiceSearch = true;
        document.body.appendChild(composebox);
        await composebox.updateComplete;

        const voiceSearch =
            composebox.shadowRoot.querySelector('cr-composebox-voice-search');
        assertTrue(!!voiceSearch);

        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        voiceSearch.shadowRoot.appendChild(style);

        const bottomActions =
            voiceSearch.shadowRoot.querySelector('#bottomActions');
        assertTrue(!!bottomActions);

        // Make sure it is initially visible (opacity 1).
        assertEquals('1', window.getComputedStyle(bottomActions).opacity);

        // Simulate voice permission prompt opening.
        composebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await composebox.updateComplete;
        await voiceSearch.updateComplete;

        // Verify the class was added and opacity turned to 0.
        assertTrue(voiceSearch.classList.contains('permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(bottomActions).opacity);
      });

  test('connectedCallback calls getSmartTabSharingActive when' +
        ' smartTabSharingVisible pre-set to true', async () => {
    handler.setResultMapperFor(
        'getSmartTabSharingActive',
        () => Promise.resolve({active: true}));

    const newComposebox = document.createElement('cr-composebox');
    newComposebox.smartTabSharingVisible = true;
    document.body.appendChild(newComposebox);
    await handler.whenCalled('getSmartTabSharingActive');
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('getSmartTabSharingActive'));
    assertTrue(newComposebox.smartTabSharingActive);
  });

  test('connectedCallback does NOT call getSmartTabSharingActive when' +
        ' smartTabSharingVisible is false', () => {
    const newComposebox = document.createElement('cr-composebox');
    newComposebox.smartTabSharingVisible = false;
    document.body.appendChild(newComposebox);

    assertEquals(0, handler.getCallCount('getSmartTabSharingActive'));
    assertFalse(newComposebox.smartTabSharingActive);
  });

  test('host template .prop binding triggers getSmartTabSharingActive' +
        ' at child mount', async () => {
    handler.setResultMapperFor(
        'getSmartTabSharingActive',
        () => Promise.resolve({active: true}));

    document.body.innerHTML = getTrustedHtml(`
      <cr-composebox smart-tab-sharing-visible></cr-composebox>
    `);

    const newComposebox =
        document.body.querySelector<ComposeboxElement>('cr-composebox');
    assertTrue(!!newComposebox);

    await handler.whenCalled('getSmartTabSharingActive');
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('getSmartTabSharingActive'));
    assertTrue(newComposebox.smartTabSharingActive);
  });

  test('UpdateAutoSuggestedTabContext_NullDoesNotDelete', () => {
    loadTimeData.overrideValues({webUIOmniboxAskGAboutThisPageEnabled: true});
    const token = {high: 0n, low: 1n} as any;
    const mockFile =
        new ComposeboxFile(token, 'Auto Tab', 'tab', InputType.kBrowserTab, {
          isDeletable: true,
          tabId: 1,
          url: 'http://example.com',
        });
    composebox.setAutomaticActiveTabForTesting(mockFile);

    let deleteFileCalled = false;
    const originalDeleteFile = composebox.deleteFile;
    composebox.deleteFile = (_uuid) => {
      deleteFileCalled = true;
      return null;
    };

    // Call with null, should NOT delete.
    composebox.updateAutoSuggestedTabContextForTesting(null);
    assertFalse(deleteFileCalled);

    // Call with different URL, SHOULD delete.
    const differentTab: TabInfo = {
      tabId: 2,
      title: 'Different Tab',
      url: 'http://different.com',
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(1)} as any,
    };
    composebox.updateAutoSuggestedTabContextForTesting(differentTab);
    assertTrue(deleteFileCalled);

    // Restore
    composebox.deleteFile = originalDeleteFile;
  });

  test('UpdateAutoSuggestedTabContext_NullDeletesIfFeatureDisabled', () => {
    loadTimeData.overrideValues({webUIOmniboxAskGAboutThisPageEnabled: false});
    const token = {high: 0n, low: 1n} as any;
    const mockFile =
        new ComposeboxFile(token, 'Auto Tab', 'tab', InputType.kBrowserTab, {
          isDeletable: true,
          tabId: 1,
          url: 'http://example.com',
        });
    composebox.setAutomaticActiveTabForTesting(mockFile);

    let deleteFileCalled = false;
    const originalDeleteFile = composebox.deleteFile;
    composebox.deleteFile = (_uuid) => {
      deleteFileCalled = true;
      return null;
    };

    // Call with null, should delete because feature is disabled.
    composebox.updateAutoSuggestedTabContextForTesting(null);
    assertTrue(deleteFileCalled);

    // Restore
    composebox.deleteFile = originalDeleteFile;
  });
});

suite('Composebox tab flyout', () => {
  let composebox: ComposeboxElement;

  setup(async () => {
    loadTimeData.overrideValues({
      'contextManagementInComposeboxEnabled': true,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await composebox.updateComplete;
  });

  test(
      'keepMenuOpenForMultiSelection is gated' +
          ' by keepMenuOpenOnTabSelectForRealbox',
      async () => {
        let openMenuCalled = false;
        composebox.getContextEntrypointElement = () => {
          return {
            openMenuForMultiSelection: () => {
              openMenuCalled = true;
            },
          } as unknown as ContextualEntrypointButtonElement;
        };

        const testElement = composebox as TestComposeboxElement;

        // Omnibox source: always returns early
        testElement.composeboxSource = 'Omnibox';
        await testElement.keepMenuOpenForMultiSelection();
        assertFalse(openMenuCalled);

        // NewTabPage source, flag off: returns early
        testElement.composeboxSource = 'NewTabPage';
        testElement.keepMenuOpenOnTabSelectForRealbox = false;
        await testElement.keepMenuOpenForMultiSelection();
        assertFalse(openMenuCalled);

        // NewTabPage source, flag on: calls openMenuForMultiSelection
        testElement.composeboxSource = 'NewTabPage';
        testElement.keepMenuOpenOnTabSelectForRealbox = true;
        await testElement.keepMenuOpenForMultiSelection();
        assertTrue(openMenuCalled);
      });

  test(
      'keepMenuOpenForMultiSelection called on add/delete tab context',
      async () => {
        let keepMenuOpenCalled = false;
        const testElement = composebox as TestComposeboxElement;
        testElement.keepMenuOpenForMultiSelection = () => {
          keepMenuOpenCalled = true;
          return Promise.resolve();
        };

        await composebox.onAddTabContext(new CustomEvent('add-tab-context', {
          detail: {
            id: 1,
            title: 'Test',
            url: 'about:blank',  // Mojo converts obj to str.
            delayUpload: false,
            origin: TabUploadOrigin.CONTEXT_MENU,
          },
        }));
        assertTrue(keepMenuOpenCalled);

        keepMenuOpenCalled = false;
        await composebox.onDeleteTabContext(
            new CustomEvent('delete-tab-context', {
              detail: {
                uuid: '0',
              },
            }));
        assertTrue(keepMenuOpenCalled);
      });

  test('onContextMenuClosed sets shareTabsFlyoutOpen to false', async () => {
    composebox.shareTabsFlyoutOpen = true;
    await composebox.onContextMenuClosed();
    assertFalse(composebox.shareTabsFlyoutOpen);
  });
});

suite('composeboxSharedMountAutoRepositionDefault', () => {
  let composebox: ComposeboxElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.resetForTesting({
      // Reuse the ComposeboxTest suite's key set, but sets
      // `composeboxShowContextMenu` to true so
      // composebox_context_menu.html.ts's
      // shared `<cr-composebox-contextual-entrypoint-and-menu>` mount renders.
      composeboxShowImageSuggest: false,
      composeboxSmartComposeEnabled: false,
      composeboxShowContextMenuDescription: false,
      composeboxShowZps: true,
      composeboxContextDragAndDropEnabled: false,
      composeboxSource: 'NTP',
      composeboxFileMaxCount: 1,
      composeboxFileMaxSize: 1024,
      composeboxAttachmentFileTypes: '.pdf',
      composeboxImageFileTypes: 'image/png',
      lensSendRawFileMediaTypesEnabled: false,
      voiceSearchCoherenceAnySearchboxExperimentEnabled: false,
      voiceSearchCoherenceSearchboxEnabled: false,
      voiceSearchCoherenceComposeboxesEnabled: false,
      composeDeepSearchPlaceholder: 'Deep Search',
      composeCreateImagePlaceholder: 'Create Image',
      searchboxComposePlaceholder: 'Compose',
      composeboxShowContextMenu: true,
      // Keys accessed by ContextualActionMenuElement class-field
      // initialization once the shared
      // `<cr-composebox-contextual-entrypoint-and-menu>` mount renders.
      // loadTimeData.getBoolean() asserts on absent keys, so these are
      // required.
      // Not optional with defaults - when `composeboxShowContextMenu` is true.
      composeboxContextMenuEnableMultiTabSelection: false,
      composeboxShowContextMenuTabPreviews: false,
      ShowContextMenuHeaders: false,
      menu: 'menu',
      addContextTitle: 'Add context',
      addContext: 'Add context',
      composeboxShowTypedSuggest: false,
      composeboxCancelButtonTitleInput: 'Cancel input',
      composeboxCancelButtonTitle: 'Cancel',
      voiceSearchButtonLabel: 'Voice search',
      lensSearchButtonLabel: 'Lens search',
      lensSearchHint: 'Lens search',
      suggestionActivityLink: '<a>Activity</a>',
      composeboxSubmitButtonTitle: 'Submit',
      composeboxSmartComposeTabTitle: 'Tab',
      composeboxSmartComposeTitle: 'Smart Compose',
      voiceListening: 'Listening',
      voiceDetails: 'Details',
      voiceClose: 'Close',
      dismissButton: 'Dismiss',
      composeboxDragAndDropHint: 'Hint',
      removeSuggestion: 'Remove',
      voiceStop: 'Stop',
      tabFaviconChipsToCoinsEnabled: false,
      removeToolChipAriaLabel: 'Remove $1',
    });

    const handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    handler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));

    const searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NTP_REALBOX'}));

    composebox = document.createElement('cr-composebox');
    composebox.showMenuOnClick = true;
    composebox.usePecApi = true;
    document.body.appendChild(composebox);
    await composebox.updateComplete;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('ShareComposeboxMountPreservesAutoReposition', async () => {
    const entrypointAndMenu = composebox.shadowRoot.querySelector<
        ContextualEntrypointAndMenuElement>(
      'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu);
    await entrypointAndMenu.updateComplete;
    assertFalse(entrypointAndMenu.disableAutoReposition);

    const contextualActionMenu = entrypointAndMenu.$.menu;
    await contextualActionMenu.updateComplete;
    const crActionMenu = contextualActionMenu.$.menu;
    assertTrue(crActionMenu.autoReposition);
    assertTrue(crActionMenu.hasAttribute('auto-reposition'));
  });

  test('voice search is absolute when listening', async () => {
    // Mock WindowProxy to enable voice search.
    const windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   addEventListener() {},
                                                   removeListener() {},
                                                   removeEventListener() {},
                                                 }));

    // Recreate composebox so updated loadTimeData and WindowProxy mock take
    // effect.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composebox = document.createElement('cr-composebox');
    composebox.showVoiceSearch = true;
    document.body.appendChild(composebox);
    await composebox.updateComplete;

    const voiceSearch =
        composebox.shadowRoot.querySelector('cr-composebox-voice-search');
    assertTrue(!!voiceSearch);
    const container = voiceSearch.shadowRoot.querySelector('#container');
    assertTrue(!!container);

    // Turn on voice search mode (sets `isListening` to true automatically).
    composebox.inVoiceSearchMode = true;
    await composebox.updateComplete;

    // Turn off listening manually to test non-listening state.
    composebox.isListening = false;
    await composebox.updateComplete;

    // Not listening yet. Should be static to influence height.
    assertEquals('static', window.getComputedStyle(voiceSearch).position);
    // Should still be absolute since permission prompt is not open and there is
    // no error.
    assertNotEquals('static', window.getComputedStyle(container).position);

    // Turn on listening manually.
    composebox.isListening = true;
    await composebox.updateComplete;

    // Should be absolute since listening is on (and no permission prompt is
    // open, and no error).
    assertEquals('absolute', window.getComputedStyle(voiceSearch).position);
    assertEquals('0px', window.getComputedStyle(voiceSearch).top);
    assertEquals('absolute', window.getComputedStyle(container).position);

    // Turn off voice search mode.
    composebox.inVoiceSearchMode = false;
    await composebox.updateComplete;

    // Since voice search mode is off, should be static.
    assertEquals('static', window.getComputedStyle(voiceSearch).position);
    // Should still be absolute since permission prompt is not open and there is
    // no error.
    assertEquals('absolute', window.getComputedStyle(container).position);
  });

  test(
      'voice permission event adds classes in animated glow and ' +
          'changes isListening and container bounds correctly',
      async () => {
        // Mock WindowProxy to enable voice search.
        const windowProxy = installMock(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
        windowProxy.setResultMapperFor('matchMedia', () => ({
                                                       addListener() {},
                                                       addEventListener() {},
                                                       removeListener() {},
                                                       removeEventListener() {},
                                                     }));

        // Recreate composebox so updated loadTimeData and WindowProxy mock take
        // effect.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;

        composebox = document.createElement('cr-composebox');
        composebox.showVoiceSearch = true;
        document.body.appendChild(composebox);
        await composebox.updateComplete;

        // Open voice search without using UI.
        composebox.inVoiceSearchMode = true;
        composebox.hasVoiceSearchError = false;
        composebox.isListening = true;
        await composebox.updateComplete;

        const voiceSearch =
            composebox.shadowRoot.querySelector('cr-composebox-voice-search');
        assertTrue(!!voiceSearch);

        const animatedGlow =
            composebox.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!animatedGlow);

        let eventFired = false;
        let eventDetail: any = null;
        composebox.addEventListener(
            'voice-permission-prompt-changed', (e: Event) => {
              eventFired = true;
              eventDetail = (e as CustomEvent).detail;
            });

        // Fire opened event:
        voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: true,
            height: 100,
            width: 100,
          },
        }));
        await composebox.updateComplete;

        // Verify that resizing occurs.
        assertTrue(eventFired);
        assertEquals(100, eventDetail.height);
        assertEquals(100, eventDetail.width);
        assertTrue(eventDetail.isOpened);
        assertFalse(composebox.isListening);
        // Verify that class to stop voice animation is added.
        assertTrue(
            animatedGlow.classList.contains('permission-prompt-showing'));

        // Reset event tracker:
        eventFired = false;
        eventDetail = null;

        // Fire closed event (query the element again in case of recreation):
        const voiceSearch2 =
            composebox.shadowRoot.querySelector('cr-composebox-voice-search');
        assertTrue(!!voiceSearch2);
        voiceSearch2.dispatchEvent(new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: false,
            height: 0,
            width: 0,
          },
        }));
        await composebox.updateComplete;

        // Verify that reverts back to original auto-size.
        assertTrue(eventFired);
        assertFalse(eventDetail.isOpened);
        assertTrue(composebox.isListening);
        // Verify that class to stop voice animation is removed.
        assertFalse(
            animatedGlow.classList.contains('permission-prompt-showing'));
      });

  test('voice permission opened event not fired if size is 0', async () => {
    // Mock WindowProxy to enable voice search.
    const windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   addEventListener() {},
                                                   removeListener() {},
                                                   removeEventListener() {},
                                                 }));

    // Recreate composebox so updated loadTimeData and WindowProxy mock take
    // effect.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composebox = document.createElement('cr-composebox');
    composebox.showVoiceSearch = true;
    document.body.appendChild(composebox);
    await composebox.updateComplete;

    // Open voice search without using UI.
    composebox.inVoiceSearchMode = true;
    composebox.hasVoiceSearchError = false;
    composebox.isListening = true;
    await composebox.updateComplete;

    const voiceSearch =
        composebox.shadowRoot.querySelector('cr-composebox-voice-search');
    assertTrue(!!voiceSearch);

    let eventFired = false;
    composebox.addEventListener('voice-permission-prompt-changed', () => {
      eventFired = true;
    });

    // Fire opened event but with height 0 width 0.
    voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
      detail: {
        isOpened: true,
        height: 0,
        width: 0,
      },
    }));
    await composebox.updateComplete;

    // Should not be fired by composebox upwards to omnibox aim app
    // because height and width are 0. No component resizing (done by
    // omnibox aim app) required.
    assertFalse(eventFired);
    // Should be 'waiting' state since a permission prompt is still open.
    assertFalse(composebox.isListening);
  });

  test('animated glow voice search slots hide when not listening', async () => {
    // Mock WindowProxy to enable voice search.
    const windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   addEventListener() {},
                                                   removeListener() {},
                                                   removeEventListener() {},
                                                 }));

    // Recreate composebox so updated loadTimeData and WindowProxy mock take
    // effect.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composebox = document.createElement('cr-composebox');
    composebox.showVoiceSearch = true;
    document.body.appendChild(composebox);
    await composebox.updateComplete;

    // Open voice search without using UI.
    composebox.voiceSearchCoherenceEnabled = true;
    composebox.inVoiceSearchMode = true;
    composebox.isListening = true;
    composebox.showFileCarousel = true;
    composebox.inToolMode = true;
    await composebox.updateComplete;

    const animatedGlow =
        composebox.shadowRoot.querySelector('search-animated-glow');
    assertTrue(!!animatedGlow);

    // Verify voice chip slots are present when listening.
    let carouselSlot =
        animatedGlow.shadowRoot.querySelector('slot[name="carousel"]');
    let toolChipSlot =
        animatedGlow.shadowRoot.querySelector('slot[name="tool-chip"]');
    assertTrue(!!carouselSlot);
    assertTrue(!!toolChipSlot);

    // Verify recording wave is present.
    let recordingWave = animatedGlow.shadowRoot.querySelector('#recordingWave');
    assertTrue(!!recordingWave);

    // Turn off listening.
    composebox.isListening = false;
    await composebox.updateComplete;

    // Verify voice chip slots are gone.
    carouselSlot =
        animatedGlow.shadowRoot.querySelector('slot[name="carousel"]');
    toolChipSlot =
        animatedGlow.shadowRoot.querySelector('slot[name="tool-chip"]');
    assertFalse(!!carouselSlot);
    assertFalse(!!toolChipSlot);

    // Verify recording wave is still present.
    recordingWave = animatedGlow.shadowRoot.querySelector('#recordingWave');
    assertTrue(!!recordingWave);
  });
});
