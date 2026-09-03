// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';

import {ComposeboxProxyImpl, getContextMenuDialog, SearchboxBrowserProxy, UnboundedMenuManager, updateUnboundedElementVisibility} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import type {OmniboxEverywhereAppElement, OmniboxEverywhereComposeboxElement, OmniboxEverywhereOmniboxElement, OmniboxEverywhereProfileIconElement, UnboundedElement} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import {ComposeboxFile, TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxState} from 'chrome://resources/cr_components/composebox/common.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {InputType} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

function getInputValue(
    inputElement: HTMLInputElement|HTMLTextAreaElement|HTMLElement): string {
  if ('value' in inputElement) {
    return (inputElement as HTMLInputElement).value;
  }
  return inputElement.innerText;
}

suite('OmniboxEverywhereOmniboxTest', () => {
  let omnibox: OmniboxEverywhereOmniboxElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      isFuseboxEnabled: true,
      searchboxVoiceSearch: true,
      searchboxLensSearch: true,
      searchboxShowComposeEntrypoint: true,
      ntpRealboxDynamicAiModeButton: true,
      composeboxContextDragAndDropEnabled: true,
      energyEffectAnimationEnabled: false,
      searchboxCr23Theming: true,
      searchboxCr23SteadyStateShadow: false,
      contextManagementInComposeboxEnabled: false,
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: false,
      searchboxLayoutMode: 'TallBottomContext',
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    omnibox = document.createElement('omnibox-everywhere-omnibox');
    document.body.appendChild(omnibox);
    await microtasksFinished();
  });

  test('AddTabContext opens composebox with tab upload', () => {
    let openComposeboxCalled = false;
    const detailHolder: {state?: ComposeboxState} = {};
    omnibox.addEventListener('open-composebox', (e: Event) => {
      openComposeboxCalled = true;
      detailHolder.state = (e as CustomEvent).detail as ComposeboxState;
    });

    const contextMenu = omnibox.shadowRoot.querySelector('#context')!;
    contextMenu.dispatchEvent(new CustomEvent('add-tab-context', {
      detail: {
        id: 123,
        title: 'Test Tab Title',
        url: 'https://example.com' as unknown as Url,
        delayUpload: false,
        origin: TabUploadOrigin.CONTEXT_MENU,
      },
      bubbles: true,
      composed: true,
    }));

    assertTrue(openComposeboxCalled);
    const files = detailHolder.state!.files;
    assertEquals(1, files.length);
    assertEquals(123, (files[0] as {tabId: number}).tabId);
    assertEquals('Test Tab Title', (files[0] as {title: string}).title);
    assertEquals('https://example.com', (files[0] as {url: Url}).url);
    assertEquals(
        TabUploadOrigin.CONTEXT_MENU,
        (files[0] as {origin: TabUploadOrigin}).origin);
  });

  test(
      'sets is-dragging-file attribute on dragenter and removes on dragleave',
      async () => {
        const inputWrapper = omnibox.shadowRoot.querySelector('#inputWrapper');
        assertTrue(!!inputWrapper);

        assertFalse(omnibox.hasAttribute('is-dragging-file'));

        inputWrapper?.dispatchEvent(new DragEvent('dragenter', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(omnibox.hasAttribute('is-dragging-file'));
        assertEquals(GlowAnimationState.DRAGGING, omnibox.animationState);

        inputWrapper?.dispatchEvent(new DragEvent('dragleave', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(omnibox.hasAttribute('is-dragging-file'));
        assertEquals(GlowAnimationState.NONE, omnibox.animationState);
      });

  test(
      'pasting files into searchbox opens composebox with pasted files', () => {
        let openComposeboxCalled = false;
        const detailHolder: {state?: ComposeboxState} = {};
        omnibox.addEventListener('open-composebox', (e: Event) => {
          openComposeboxCalled = true;
          detailHolder.state = (e as CustomEvent).detail as ComposeboxState;
        });

        const file = new File(['foo'], 'foo.png', {type: 'image/png'});
        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(file);

        const input = omnibox.shadowRoot.querySelector('#input')!;
        input.dispatchEvent(new CustomEvent('searchbox-input-files-pasted', {
          detail: {files: dataTransfer.files},
          bubbles: true,
          composed: true,
        }));

        assertTrue(openComposeboxCalled);
        const files = detailHolder.state!.files;
        assertEquals(1, files.length);
        assertEquals(file, (files[0] as {file: File}).file);
      });

  test(
      'clicking voice search button dispatches open-voice-search event',
      async () => {
        let eventFired = false;
        omnibox.addEventListener('open-voice-search', () => {
          eventFired = true;
        });

        const voiceBtn = omnibox.shadowRoot.querySelector<HTMLElement>(
            '#voiceSearchButton')!;
        assertTrue(!!voiceBtn);
        voiceBtn.click();
        await microtasksFinished();

        assertTrue(eventFired);
      });

  test(
      'configures animated glow and compose button properties correctly',
      () => {
        const glow =
            omnibox.shadowRoot.querySelector<SearchAnimatedGlowElement>(
                'search-animated-glow');
        assertTrue(!!glow);
        assertEquals('OmniboxEverywhere', glow.entrypointName);

        const composeButton =
            omnibox.shadowRoot.querySelector('#composeButton');
        assertTrue(!!composeButton);
      });

  test(
      'updates has-user-input on compose button when text changes',
      async () => {
        const composeButton =
            omnibox.shadowRoot.querySelector('#composeButton')!;
        assertTrue(!!composeButton);
        assertFalse(composeButton.hasAttribute('has-user-input'));

        const input = omnibox.shadowRoot.querySelector('#input')!;
        input.dispatchEvent(new CustomEvent('searchbox-input-text-updated', {
          detail: {value: 'test query', isComposing: false},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(composeButton.hasAttribute('has-user-input'));

        input.dispatchEvent(new CustomEvent('searchbox-input-text-updated', {
          detail: {value: '', isComposing: false},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(composeButton.hasAttribute('has-user-input'));
      });

  test(
      'clicking compose button with empty input dispatches open-composebox ' +
          'event',
      async () => {
        const whenOpenComposebox = eventToPromise('open-composebox', omnibox);

        const composeButton =
            omnibox.shadowRoot.querySelector<HTMLElement>('#composeButton')!;
        assertTrue(!!composeButton);
        composeButton.dispatchEvent(new CustomEvent('compose-click', {
          bubbles: true,
          composed: true,
          detail: {
            button: 0,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
        }));

        await whenOpenComposebox;
        assertEquals(0, testProxy.handler.getCallCount('submitQuery'));
      });

  test(
      'clicking compose button with query text submits query and notifies ' +
          'session',
      async () => {
        let openComposeboxCalled = false;
        omnibox.addEventListener('open-composebox', () => {
          openComposeboxCalled = true;
        });

        omnibox.setInputText('test query');
        await microtasksFinished();

        const composeButton =
            omnibox.shadowRoot.querySelector<HTMLElement>('#composeButton')!;
        assertTrue(!!composeButton);
        assertTrue(composeButton.hasAttribute('has-user-input'));

        composeButton.dispatchEvent(new CustomEvent('compose-click', {
          bubbles: true,
          composed: true,
          detail: {
            button: 0,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
        }));

        await testProxy.handler.whenCalled('submitQuery');
        assertEquals(1, testProxy.handler.getCallCount('submitQuery'));
        assertEquals(1, testProxy.handler.getCallCount('notifySessionStarted'));
        assertEquals(
            1, testProxy.handler.getCallCount('activateMetricsFunnel'));
        const submitArgs = testProxy.handler.getArgs('submitQuery')[0];
        assertEquals('test query', submitArgs[0]);
        assertEquals(0, submitArgs[1]);      // button
        assertEquals(false, submitArgs[2]);  // altKey
        assertEquals(false, submitArgs[3]);  // ctrlKey
        assertEquals(false, submitArgs[4]);  // metaKey
        assertEquals(false, submitArgs[5]);  // shiftKey
        assertEquals(false, submitArgs[6]);  // isVoiceSearch
        assertFalse(openComposeboxCalled);
        assertEquals('', omnibox.$.input.inputElement.value);
        assertFalse(composeButton.hasAttribute('has-user-input'));
      });

  test('navigateToMatch clears input text on keyboard navigation', async () => {
    omnibox.setInputText('query');
    omnibox.activeQueryId = 0;
    omnibox.lastQueriedInput = 'query';
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: 0,
          input: 'query',
          matches: [
            createSearchMatchForTesting({
              allowedToBeDefaultMatch: true,
              fillIntoEdit: 'query match',
            }),
          ],
        }));
    await microtasksFinished();

    const keyboardEvent = new KeyboardEvent('keydown', {
      key: 'Enter',
      cancelable: true,
    });
    omnibox.navigateToMatch(0, keyboardEvent);
    await microtasksFinished();

    assertEquals('', omnibox.$.input.inputElement.value);
  });

  test('openCtrlEnterMatch clears input text', async () => {
    omnibox.setInputText('query');
    omnibox.activeQueryId = 0;
    omnibox.lastQueriedInput = 'query';
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: 0,
          input: 'query',
          matches: [
            createSearchMatchForTesting({
              allowedToBeDefaultMatch: true,
              fillIntoEdit: 'query match',
            }),
          ],
        }));
    await microtasksFinished();

    omnibox.openCtrlEnterMatch(0);
    await microtasksFinished();

    assertEquals('', omnibox.$.input.inputElement.value);
  });

  test('onMatchClick clears input text', async () => {
    omnibox.setInputText('query');
    await microtasksFinished();
    assertEquals('query', omnibox.$.input.inputElement.value);

    omnibox.onMatchClick();
    await microtasksFinished();

    assertEquals('', omnibox.$.input.inputElement.value);
  });

  test('respects isFuseboxEnabled false', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      isFuseboxEnabled: false,
      searchboxVoiceSearch: true,
      searchboxLensSearch: true,
    });
    const element = document.createElement('omnibox-everywhere-omnibox');
    document.body.appendChild(element);
    await microtasksFinished();

    assertFalse(!!element.shadowRoot.querySelector('#context'));
    assertFalse(!!element.shadowRoot.querySelector('#lensSearchButton'));
    assertTrue(!!element.shadowRoot.querySelector('#voiceSearchButton'));
  });

  test(
      'updateAimPopupEligibility toggles compose button and plus button',
      async () => {
        assertTrue(!!omnibox.shadowRoot.querySelector('#composeButton'));
        assertTrue(!!omnibox.shadowRoot.querySelector('#context'));

        testProxy.page.updateAimPopupEligibility(false);
        await microtasksFinished();

        assertFalse(!!omnibox.shadowRoot.querySelector('#composeButton'));
        assertFalse(!!omnibox.shadowRoot.querySelector('#context'));
        assertTrue(!!omnibox.shadowRoot.querySelector('#voiceSearchButton'));

        testProxy.page.updateAimPopupEligibility(true);
        await microtasksFinished();

        assertTrue(!!omnibox.shadowRoot.querySelector('#composeButton'));
        assertTrue(!!omnibox.shadowRoot.querySelector('#context'));
      });

  test(
      'updateAimPopupEligibility respects isFuseboxEnabled false capability ' +
          'in loadTimeData',
      async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({
          isFuseboxEnabled: false,
          searchboxShowComposeEntrypoint: true,
        });
        const testOmnibox =
            document.createElement('omnibox-everywhere-omnibox');
        document.body.appendChild(testOmnibox);
        await microtasksFinished();

        assertTrue(!!testOmnibox.shadowRoot.querySelector('#composeButton'));
        assertFalse(!!testOmnibox.shadowRoot.querySelector('#context'));

        testProxy.page.updateAimPopupEligibility(false);
        await microtasksFinished();

        assertFalse(!!testOmnibox.shadowRoot.querySelector('#composeButton'));
        assertFalse(!!testOmnibox.shadowRoot.querySelector('#context'));

        testProxy.page.updateAimPopupEligibility(true);
        await microtasksFinished();

        assertTrue(!!testOmnibox.shadowRoot.querySelector('#composeButton'));
        assertFalse(!!testOmnibox.shadowRoot.querySelector('#context'));
      });

  test('ContextMenuUnboundedToggleEvent', async () => {
    const contextMenu =
        omnibox.shadowRoot.querySelector<ContextualEntrypointAndMenuElement>(
            '#context')!;
    const mockDialog = document.createElement('dialog') as UnboundedElement;
    mockDialog.id = 'dialog';
    // Dialog must be attached to the DOM so Blink tracks its open state in
    // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
    document.body.appendChild(mockDialog);
    (contextMenu as unknown as {getDialog: () => HTMLDialogElement}).getDialog =
        () => mockDialog;

    let closeMenuCalled = false;
    contextMenu.closeMenu = () => {
      closeMenuCalled = true;
    };

    contextMenu.dispatchEvent(new CustomEvent('context-menu-opened'));
    await omnibox.updateComplete;

    const event = new ToggleEvent('beforetoggle', {
      oldState: 'open',
      newState: 'closed',
    });
    mockDialog.dispatchEvent(event);

    assertTrue(closeMenuCalled);
    mockDialog.remove();
  });

  test('ContextMenuClosed event removes unbounded visibility', async () => {
    const contextMenu =
        omnibox.shadowRoot.querySelector<ContextualEntrypointAndMenuElement>(
            '#context')!;
    const mockDialog = document.createElement('dialog') as UnboundedElement;
    mockDialog.id = 'dialog';
    // Dialog must be attached to the DOM so Blink tracks its open state in
    // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
    document.body.appendChild(mockDialog);
    mockDialog.setAttribute('unbounded', '');
    let hideCalled = false;
    mockDialog.hideUnboundedElement = () => {
      hideCalled = true;
      return Promise.resolve();
    };
    (contextMenu as unknown as {getDialog: () => HTMLDialogElement}).getDialog =
        () => mockDialog;

    contextMenu.dispatchEvent(new CustomEvent('context-menu-closed'));
    await microtasksFinished();

    assertTrue(hideCalled);
    assertFalse(mockDialog.hasAttribute('unbounded'));
    mockDialog.remove();
  });

  test('onInputWrapperFocusout ignores blur when dialog is open', () => {
    const contextMenu =
        omnibox.shadowRoot.querySelector<ContextualEntrypointAndMenuElement>(
            '#context')!;
    const mockDialog = document.createElement('dialog') as UnboundedElement;
    mockDialog.id = 'dialog';
    // Dialog must be attached to the DOM before mutating `open` so Blink tracks
    // it in AllOpenDialogs(), avoiding DCHECK failures when resetting `open`.
    document.body.appendChild(mockDialog);
    mockDialog.open = true;
    (contextMenu as unknown as {getDialog: () => HTMLDialogElement}).getDialog =
        () => mockDialog;

    let superFocusoutCalled = false;
    const originalFocusout =
        Object.getPrototypeOf(Object.getPrototypeOf(omnibox))
            .onInputWrapperFocusout;
    try {
      Object.getPrototypeOf(Object.getPrototypeOf(omnibox))
          .onInputWrapperFocusout = () => {
        superFocusoutCalled = true;
      };

      omnibox.onInputWrapperFocusout(new FocusEvent('focusout'));
      assertFalse(superFocusoutCalled);

      mockDialog.open = false;
      omnibox.onInputWrapperFocusout(new FocusEvent('focusout'));
      assertTrue(superFocusoutCalled);
    } finally {
      Object.getPrototypeOf(Object.getPrototypeOf(omnibox))
          .onInputWrapperFocusout = originalFocusout;
      mockDialog.remove();
    }
  });

  test('dropdownIsVisible preserves bottomControls', async () => {
    const bottomControls =
        omnibox.shadowRoot.querySelector<HTMLElement>('#bottomControls');
    assertTrue(!!bottomControls);
    assertFalse(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);

    omnibox.dropdownIsVisible = true;
    await microtasksFinished();

    assertTrue(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);

    omnibox.dropdownIsVisible = false;
    await microtasksFinished();

    assertFalse(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);
  });

  test('balanced layout and element clearances', () => {
    const inputWrapper =
        omnibox.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!inputWrapper);
    const wrapperStyle = window.getComputedStyle(inputWrapper);
    assertEquals('28px', wrapperStyle.borderRadius);
    assertEquals('6px', wrapperStyle.paddingTop);

    const bottomControls =
        omnibox.shadowRoot.querySelector<HTMLElement>('#bottomControls');
    assertTrue(!!bottomControls);
    const bottomControlsStyle = window.getComputedStyle(bottomControls);
    assertEquals('4px', bottomControlsStyle.paddingTop);
    assertEquals('12px', bottomControlsStyle.paddingBottom);

    const composeButton =
        omnibox.shadowRoot.querySelector<HTMLElement>('#composeButton');
    assertTrue(!!composeButton);
    assertEquals('12px', window.getComputedStyle(composeButton).top);

    const profileIcon =
        omnibox.shadowRoot.querySelector<HTMLElement>('#profileIcon');
    assertTrue(!!profileIcon);
    assertEquals('12px', window.getComputedStyle(profileIcon).top);
  });

  test(
      'clicking lens button calls showScreenshotMenu and sets ' +
          'isScreenshotMenuOpen',
      async () => {
        const lensButton =
            omnibox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
        assertTrue(!!lensButton);
        assertFalse(omnibox.isScreenshotMenuOpen);

        lensButton.click();
        await microtasksFinished();

        assertTrue(omnibox.isScreenshotMenuOpen);
        const lensContainer = omnibox.shadowRoot.querySelector(
            '.searchbox-icon-button-container.lens');
        assertTrue(
            !!lensContainer && lensContainer.classList.contains('menu-open'));

        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));
        const args = testProxy.handler.getArgs('showScreenshotMenu')[0];
        assertTrue(args !== undefined);

        testProxy.page.onScreenshotMenuClosed();
        await microtasksFinished();

        assertFalse(omnibox.isScreenshotMenuOpen);
        assertFalse(lensContainer.classList.contains('menu-open'));
      });
});


suite('OmniboxEverywhereComposeboxTest', () => {
  let composebox: OmniboxEverywhereComposeboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let mockPageHandler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      composeboxContextDragAndDropEnabled: true,
      energyEffectAnimationEnabled: false,
      composeboxEnergyEffectAnimationEnabled: true,
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: false,
      searchboxLayoutMode: 'TallBottomContext',
      composeboxCancelButtonTitle: 'Close AI Mode',
      composeboxCancelButtonTitleInput: 'Clear text',
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockPageHandler,
        testProxy.handler as unknown as SearchboxPageHandlerRemote,
        testProxy.callbackRouter as unknown as SearchboxPageCallbackRouter));

    composebox = document.createElement('omnibox-everywhere-composebox');
    document.body.appendChild(composebox);
    await microtasksFinished();
  });

  test('configures animated glow on composebox correctly', () => {
    const glow = composebox.shadowRoot.querySelector<SearchAnimatedGlowElement>(
        '#animatedSearchElement');
    assertTrue(!!glow);
    assertEquals('OmniboxEverywhere', glow.entrypointName);
    assertEquals('expanding', glow.animationState);
    assertTrue(glow.energyEffectAnimationEnabled);
  });

  test('AddTabContext event adds tab to composebox files', async () => {
    const mockToken = {high: 1234n, low: 5678n};
    testProxy.handler.setPromiseResolveFor('addTabContext', mockToken);

    const contextMenu =
        composebox.shadowRoot.querySelector('#contextEntrypoint')!;
    contextMenu.dispatchEvent(new CustomEvent('add-tab-context', {
      detail: {
        id: 789,
        title: 'Composebox Direct Tab',
        url: 'https://direct.com' as unknown as Url,
        delayUpload: false,
        origin: TabUploadOrigin.CONTEXT_MENU,
      },
      bubbles: true,
      composed: true,
    }));

    await testProxy.handler.whenCalled('addTabContext');
    const args = testProxy.handler.getArgs('addTabContext')[0];
    assertEquals(789, args[0]);
    assertFalse(args[1]);

    await microtasksFinished();
    assertEquals(1, composebox.attachedContext.size);
    const file = Array.from(composebox.attachedContext.values())[0]!;
    assertEquals(789, file.tabId);
    assertEquals('Composebox Direct Tab', file.name);
  });

  test(
      'clicking voice search button dispatches open-voice-search event',
      async () => {
        composebox.showVoiceSearch = true;
        await microtasksFinished();

        let eventFired = false;
        composebox.addEventListener('open-voice-search', () => {
          eventFired = true;
        });

        const voiceBtn = composebox.shadowRoot.querySelector<HTMLElement>(
            '#voiceSearchButton')!;
        assertTrue(!!voiceBtn);
        voiceBtn.click();
        await microtasksFinished();

        assertTrue(eventFired);
      });

  test('setInputText sets composebox input value', async () => {
    composebox.setInputText('test composebox query');
    await microtasksFinished();
    assertEquals('test composebox query', composebox.getInputElement().input);
  });

  test('submitting query clears composebox input', async () => {
    composebox.setInputText('test composebox query');
    await microtasksFinished();
    assertEquals('test composebox query', composebox.getInputElement().input);

    composebox.submitQuery();
    await microtasksFinished();

    assertEquals('', composebox.input);
    assertEquals('', composebox.getInputElement().input);
  });

  test(
      'sets is-dragging-file attribute on dragenter and removes on dragleave',
      async () => {
        const dropZone = composebox.shadowRoot.querySelector('#composebox');
        assertTrue(!!dropZone);

        assertFalse(composebox.hasAttribute('is-dragging-file'));

        dropZone?.dispatchEvent(new DragEvent('dragenter', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(composebox.hasAttribute('is-dragging-file'));

        dropZone?.dispatchEvent(new DragEvent('dragleave', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(composebox.hasAttribute('is-dragging-file'));
      });

  test('pasting files into composebox processes files', async () => {
    const mockToken = {high: 4567n, low: 8910n};
    testProxy.handler.setPromiseResolveFor('addFileContext', mockToken);

    const file = new File(['test content'], 'test.png', {type: 'image/png'});
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);

    const pasteEvent = new CustomEvent('paste', {
                         bubbles: true,
                         composed: true,
                       }) as unknown as ClipboardEvent;
    Object.defineProperty(pasteEvent, 'clipboardData', {
      value: dataTransfer,
    });

    const dropZone = composebox.shadowRoot.querySelector('#composebox')!;
    assertTrue(!!dropZone);
    dropZone.dispatchEvent(pasteEvent);

    await testProxy.handler.whenCalled('addFileContext');
    assertEquals(1, testProxy.handler.getCallCount('addFileContext'));
    await microtasksFinished();
    assertEquals(1, composebox.attachedContext.size);
  });

  test('ContextMenuUnboundedToggleEvent', async () => {
    let closeMenuCalled = false;
    const contextMenu = composebox.getContextEntrypointElement() as
        ContextualEntrypointAndMenuElement;
    contextMenu.closeMenu = () => {
      closeMenuCalled = true;
    };

    const mockDialog = document.createElement('dialog') as UnboundedElement;
    mockDialog.id = 'dialog';
    // Dialog must be attached to the DOM so Blink tracks its open state in
    // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
    document.body.appendChild(mockDialog);
    (contextMenu as unknown as {getDialog: () => HTMLDialogElement}).getDialog =
        () => mockDialog;

    composebox.onContextMenuOpened();
    await composebox.updateComplete;

    const event = new ToggleEvent('beforetoggle', {
      oldState: 'open',
      newState: 'closed',
    });
    mockDialog.dispatchEvent(event);

    assertTrue(closeMenuCalled);
    mockDialog.remove();
  });

  test('computeShowDropdown returns true when dialog is open', () => {
    const contextMenu = composebox.getContextEntrypointElement() as
        ContextualEntrypointAndMenuElement;
    const mockDialog = document.createElement('dialog') as UnboundedElement;
    mockDialog.id = 'dialog';
    // Dialog must be attached to the DOM before mutating `open` so Blink tracks
    // it in AllOpenDialogs(), avoiding DCHECK failures when resetting `open`.
    document.body.appendChild(mockDialog);
    mockDialog.open = true;
    (contextMenu as unknown as {getDialog: () => HTMLDialogElement}).getDialog =
        () => mockDialog;

    assertTrue(composebox.computeShowDropdown());

    mockDialog.open = false;
    composebox.showDropdown = false;
    assertFalse(composebox.computeShowDropdown());
    mockDialog.remove();
  });

  test('onContextMenuClosed removes unbounded visibility', async () => {
    const contextMenu = composebox.getContextEntrypointElement() as
        ContextualEntrypointAndMenuElement;
    const mockDialog = document.createElement('dialog') as UnboundedElement;
    mockDialog.id = 'dialog';
    // Dialog must be attached to the DOM so Blink tracks its open state in
    // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
    document.body.appendChild(mockDialog);
    mockDialog.setAttribute('unbounded', '');
    let hideCalled = false;
    mockDialog.hideUnboundedElement = () => {
      hideCalled = true;
      return Promise.resolve();
    };
    (contextMenu as unknown as {getDialog: () => HTMLDialogElement}).getDialog =
        () => mockDialog;

    await composebox.onContextMenuClosed();
    await microtasksFinished();

    assertTrue(hideCalled);
    assertFalse(mockDialog.hasAttribute('unbounded'));
    mockDialog.remove();
  });

  test('cancel button title reflects input and file state', async () => {
    const cancelIcon =
        composebox.getInputElement().shadowRoot.querySelector<HTMLElement>(
            '#cancelIcon')!;
    assertTrue(!!cancelIcon);
    assertEquals('Close AI Mode', cancelIcon.getAttribute('title'));

    composebox.input = 'search query';
    await composebox.updateComplete;
    await microtasksFinished();
    assertEquals('Clear text', cancelIcon.getAttribute('title'));

    composebox.input = '';
    const mockToken = 'mock-token-uuid';
    const file = new ComposeboxFile(
        mockToken, 'test.png', 'image/png', InputType.kLensImage);
    composebox.attachedContext.set(mockToken, file);
    composebox.attachedContext = new Map(composebox.attachedContext);
    await composebox.updateComplete;
    await microtasksFinished();
    assertEquals('Clear text', cancelIcon.getAttribute('title'));
  });

  test('cancel button clears input text when there is text', async () => {
    composebox.input = 'some query';
    await composebox.updateComplete;
    await microtasksFinished();

    let closeEventFired = false;
    composebox.addEventListener('close-composebox', () => {
      closeEventFired = true;
    });

    const cancelIcon =
        composebox.getInputElement().shadowRoot.querySelector<HTMLElement>(
            '#cancelIcon')!;
    cancelIcon.click();
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals('', composebox.input);
    assertEquals(1, testProxy.handler.getCallCount('clearFiles'));
    assertFalse(closeEventFired);
  });

  test('cancel button clears files when there are files', async () => {
    const mockToken = 'mock-token-uuid';
    const file = new ComposeboxFile(
        mockToken, 'test.png', 'image/png', InputType.kLensImage);
    composebox.attachedContext.set(mockToken, file);
    composebox.attachedContext = new Map(composebox.attachedContext);
    await composebox.updateComplete;
    await microtasksFinished();

    let closeEventFired = false;
    composebox.addEventListener('close-composebox', () => {
      closeEventFired = true;
    });

    const cancelIcon =
        composebox.getInputElement().shadowRoot.querySelector<HTMLElement>(
            '#cancelIcon')!;
    cancelIcon.click();
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(0, composebox.attachedContext.size);
    assertEquals(1, testProxy.handler.getCallCount('clearFiles'));
    assertFalse(closeEventFired);
  });

  test(
      'cancel button fires close-composebox when composebox is empty',
      async () => {
        let closeEventFired = false;
        composebox.addEventListener('close-composebox', () => {
          closeEventFired = true;
        });

        const cancelIcon =
            composebox.getInputElement().shadowRoot.querySelector<HTMLElement>(
                '#cancelIcon')!;
        cancelIcon.click();
        await composebox.updateComplete;
        await microtasksFinished();

        assertTrue(closeEventFired);
        assertEquals(1, testProxy.handler.getCallCount('clearFiles'));
      });

  test('getFileInputsElement returns element or null when disabled', () => {
    assertEquals(composebox.$.fileInputs, composebox.getFileInputsElement());
    composebox.contextMenuEnabled = false;
    assertEquals(null, composebox.getFileInputsElement());
  });

  test(
      'clicking lens button in composebox calls showScreenshotMenu and sets ' +
          'isScreenshotMenuOpen',
      async () => {
        const lensButton = composebox.shadowRoot.querySelector<HTMLElement>(
            '#lensSearchButton');
        assertTrue(!!lensButton);
        assertFalse(composebox.isScreenshotMenuOpen);

        lensButton.click();
        await microtasksFinished();

        assertTrue(composebox.isScreenshotMenuOpen);
        const lensContainer = composebox.shadowRoot.querySelector(
            '.searchbox-icon-button-container.lens');
        assertTrue(
            !!lensContainer && lensContainer.classList.contains('menu-open'));

        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));
        const args = testProxy.handler.getArgs('showScreenshotMenu')[0];
        assertTrue(args !== undefined);

        testProxy.page.onScreenshotMenuClosed();
        await microtasksFinished();

        assertFalse(composebox.isScreenshotMenuOpen);
        assertFalse(lensContainer.classList.contains('menu-open'));
      });
});

suite('UnboundedUtilsTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('getContextMenuDialog resolves nested dialog correctly', () => {
    const host = document.createElement('div');
    const hostRoot = host.attachShadow({mode: 'open'});
    const entrypoint = document.createElement('div');
    entrypoint.id = 'context';
    const entrypointRoot = entrypoint.attachShadow({mode: 'open'});
    const menu1 = document.createElement('div');
    menu1.id = 'menu';
    const menu1Root = menu1.attachShadow({mode: 'open'});
    const menu2 = document.createElement('div');
    menu2.id = 'menu';
    const menu2Root = menu2.attachShadow({mode: 'open'});
    const dialog = document.createElement('dialog') as UnboundedElement;
    dialog.id = 'dialog';

    menu2Root.appendChild(dialog);
    menu1Root.appendChild(menu2);
    entrypointRoot.appendChild(menu1);
    hostRoot.appendChild(entrypoint);
    document.body.appendChild(host);

    assertEquals(dialog, getContextMenuDialog(hostRoot, '#context'));
    assertEquals(null, getContextMenuDialog(null, '#context'));
    assertEquals(null, getContextMenuDialog(hostRoot, '#nonExistent'));
  });

  test('UnboundedMenuManager manages lifecycle cleanly', async () => {
    const mockDialog = document.createElement('dialog') as UnboundedElement;
    // Dialog must be attached to the DOM so Blink tracks its open state in
    // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
    document.body.appendChild(mockDialog);
    let showCalled = false;
    let hideCalled = false;
    mockDialog.showUnboundedElement = () => {
      showCalled = true;
      return Promise.resolve();
    };
    mockDialog.hideUnboundedElement = () => {
      hideCalled = true;
      return Promise.resolve();
    };

    const host = document.createElement('div');
    (host as unknown as {getDialog: () => HTMLDialogElement}).getDialog = () =>
        mockDialog;

    let closedFired = false;
    const manager = new UnboundedMenuManager(() => host, () => {
      closedFired = true;
    });

    assertEquals(mockDialog, manager.getDialog());
    assertFalse(manager.isDialogOpen());

    manager.onContextMenuOpened();
    assertTrue(showCalled);

    const event = new ToggleEvent('beforetoggle', {
      oldState: 'open',
      newState: 'closed',
    });
    mockDialog.dispatchEvent(event);
    assertTrue(closedFired);

    manager.onContextMenuClosed();
    await microtasksFinished();
    assertTrue(hideCalled);
    mockDialog.remove();
  });

  test('updateUnboundedElementVisibility show and hide', async () => {
    const dialog = document.createElement('dialog') as UnboundedElement;
    // Dialog must be attached to the DOM so Blink tracks its open state in
    // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
    document.body.appendChild(dialog);
    let showCalled = false;
    let hideCalled = false;
    dialog.showUnboundedElement = () => {
      showCalled = true;
      return Promise.resolve();
    };
    dialog.hideUnboundedElement = () => {
      hideCalled = true;
      return Promise.resolve();
    };

    updateUnboundedElementVisibility(dialog, true);
    assertTrue(dialog.hasAttribute('unbounded'));
    assertTrue(showCalled);

    updateUnboundedElementVisibility(dialog, false);
    await microtasksFinished();
    assertTrue(hideCalled);
    assertFalse(dialog.hasAttribute('unbounded'));
    dialog.remove();
  });

  test(
      'updateUnboundedElementVisibility handles async check and failures',
      async () => {
        const dialog = document.createElement('dialog') as UnboundedElement;
        // Dialog must be attached to the DOM so Blink tracks its open state in
        // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
        document.body.appendChild(dialog);
        let showCallCount = 0;
        dialog.showUnboundedElement = () => {
          showCallCount++;
          return Promise.resolve();
        };

        // Async check returns false -> should not call showUnboundedElement.
        updateUnboundedElementVisibility(dialog, true, () => false);
        await new Promise(resolve => requestAnimationFrame(resolve));
        assertEquals(0, showCallCount);
        assertFalse(dialog.hasAttribute('unbounded'));

        // Async check returns true -> should call showUnboundedElement.
        updateUnboundedElementVisibility(dialog, true, () => true);
        await new Promise(resolve => requestAnimationFrame(resolve));
        assertEquals(1, showCallCount);

        // Hide with rejection cleans up attribute.
        dialog.setAttribute('unbounded', '');
        dialog.hideUnboundedElement = () =>
            Promise.reject(new Error('Native error'));
        updateUnboundedElementVisibility(dialog, false);
        await microtasksFinished();
        assertFalse(dialog.hasAttribute('unbounded'));
        dialog.remove();
      });
});


declare global {
  interface Window {
    webkitSpeechRecognition: unknown;
  }
}

class MockSpeechRecognition {
  start() {}
  stop() {}
  abort() {}
}

suite('OmniboxEverywhereAppTest', () => {
  let app: OmniboxEverywhereAppElement;
  let testProxy: TestSearchboxBrowserProxy;
  let mockPageHandler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.webkitSpeechRecognition = MockSpeechRecognition;

    loadTimeData.overrideValues({
      isFuseboxEnabled: true,
      searchboxVoiceSearch: true,
      searchboxLensSearch: true,
      omniboxPopupDebugEnabled: false,
      searchboxLayoutMode: 'normal',
      caretAnimationEnabled: true,
      composeboxAnimationDisabled: false,
      contextualMenuUsePecApi: false,
      contextButtonShapeIsOblong: false,
      contextManagementInComposeboxEnabled: false,
      searchboxCr23Theming: true,
      searchboxCr23SteadyStateShadow: false,
      searchboxShowComposeEntrypoint: false,
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: false,
      omniboxEverywhereShowShortcuts: true,
      initialShowFre: false,
      composeboxCancelButtonTitle: 'Close AI Mode',
      composeboxCancelButtonTitleInput: 'Clear text',
    });

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockPageHandler,
        testProxy.handler as unknown as SearchboxPageHandlerRemote,
        testProxy.callbackRouter as unknown as SearchboxPageCallbackRouter));

    app = document.createElement('omnibox-everywhere-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });


  test('open-voice-search opens voice search dialog overlay', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    searchbox.dispatchEvent(
        new CustomEvent('open-voice-search', {bubbles: true, composed: true}));
    await microtasksFinished();

    const dialog =
        app.shadowRoot.querySelector<HTMLDialogElement>('#voiceSearchDialog');
    assertTrue(!!dialog);
    const voiceSearch = app.shadowRoot.querySelector('#voiceSearch');
    assertTrue(!!voiceSearch);
  });

  test(
      'clicking voice search button opens voice search dialog overlay and handles permission prompt',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        const voiceBtn = searchbox.shadowRoot.querySelector<HTMLElement>(
            '#voiceSearchButton')!;
        assertTrue(!!voiceBtn);
        voiceBtn.click();
        await microtasksFinished();

        const dialog = app.shadowRoot.querySelector<HTMLDialogElement>(
            '#voiceSearchDialog');
        assertTrue(!!dialog);
        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);
        const glow = app.shadowRoot.querySelector<SearchAnimatedGlowElement>(
            '#voiceSearchGlow');
        assertTrue(!!glow);

        // Verify permission prompt showing state is handled.
        voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: true,
            width: 100,
            height: 200,
          },
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertTrue(voiceSearch.classList.contains('permission-prompt-showing'));
        assertTrue(glow.classList.contains('permission-prompt-showing'));

        // Verify permission prompt closed state is handled.
        voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: false,
            width: 0,
            height: 0,
          },
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertFalse(
            voiceSearch.classList.contains('permission-prompt-showing'));
        assertFalse(glow.classList.contains('permission-prompt-showing'));
      });

  test(
      'clicking voice search button in composebox opens voice search dialog' +
          ' overlay and handles permission prompt',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.dispatchEvent(new CustomEvent('open-composebox', {
          detail: {
            text: '',
            files: [],
            mode: 0,
            model: 0,
          },
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);
        const voiceBtn = composebox.shadowRoot.querySelector<HTMLElement>(
            '#voiceSearchButton')!;
        assertTrue(!!voiceBtn);
        voiceBtn.click();
        await microtasksFinished();

        const dialog = app.shadowRoot.querySelector<HTMLDialogElement>(
            '#voiceSearchDialog');
        assertTrue(!!dialog);
        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);
        const glow = app.shadowRoot.querySelector<SearchAnimatedGlowElement>(
            '#voiceSearchGlow');
        assertTrue(!!glow);

        // Verify permission prompt showing state is handled in composebox mode.
        voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: true,
            width: 100,
            height: 200,
          },
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertTrue(voiceSearch.classList.contains('permission-prompt-showing'));
        assertTrue(glow.classList.contains('permission-prompt-showing'));

        // Verify permission prompt closed state is handled in composebox mode.
        voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: false,
            width: 0,
            height: 0,
          },
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertFalse(
            voiceSearch.classList.contains('permission-prompt-showing'));
        assertFalse(glow.classList.contains('permission-prompt-showing'));
      });

  test(
      'voice search final result submits query and closes dialog', async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);

        voiceSearch.dispatchEvent(new CustomEvent('voice-search-final-result', {
          detail: 'test query from speech',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const dialog = app.shadowRoot.querySelector('#voiceSearchDialog');
        assertFalse(!!dialog);

        assertTrue(!!searchbox);
        assertEquals('', searchbox.$.input.inputElement.value);

        await testProxy.handler.whenCalled('submitQuery');
        const args = testProxy.handler.getArgs('submitQuery')[0];
        assertEquals('test query from speech', args[0]);
        assertEquals(0, args[1]);  // mouse_button
        assertFalse(args[2]);      // alt_key
        assertFalse(args[3]);      // ctrl_key
        assertFalse(args[4]);      // meta_key
        assertFalse(args[5]);      // shift_key
        assertTrue(args[6]);       // is_voice_search
      });

  test('submitting composebox switches out of composebox mode', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    searchbox.setInputText('searchbox text');
    searchbox.dispatchEvent(new CustomEvent('open-composebox', {
      detail: {text: 'searchbox text', files: [], mode: 0, model: 0},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    const composebox =
        app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
    assertTrue(!!composebox);

    composebox.dispatchEvent(new CustomEvent('composebox-submit', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertFalse(
        !!app.shadowRoot.querySelector('omnibox-everywhere-composebox'));
    const restoredSearchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    assertTrue(!!restoredSearchbox);
    assertEquals('', restoredSearchbox.$.input.inputElement.value);
  });

  test(
      'voice search final result in composebox submits query and closes dialog',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.dispatchEvent(new CustomEvent('open-composebox', {
          detail: {text: '', files: [], mode: 0, model: 0},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        composebox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);

        voiceSearch.dispatchEvent(new CustomEvent('voice-search-final-result', {
          detail: 'composebox speech query',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const dialog = app.shadowRoot.querySelector('#voiceSearchDialog');
        assertFalse(!!dialog);

        await testProxy.handler.whenCalled('submitQuery');
        const args = testProxy.handler.getArgs('submitQuery')[0];
        assertEquals('composebox speech query', args[0]);
        assertTrue(args[6]);  // is_voice_search
      });

  test(
      'stopping voice search fills input plate without submitting',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);

        voiceSearch.dispatchEvent(new CustomEvent('recording-stopped', {
          detail: 'stopped speech query',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const dialog = app.shadowRoot.querySelector('#voiceSearchDialog');
        assertFalse(!!dialog);

        assertEquals(
            'stopped speech query', searchbox.$.input.inputElement.value);
        assertEquals(0, testProxy.handler.getCallCount('submitQuery'));
      });

  test(
      'stopping voice search in composebox fills input plate without ' +
          'submitting',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.dispatchEvent(new CustomEvent('open-composebox', {
          detail: {text: '', files: [], mode: 0, model: 0},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        composebox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);

        voiceSearch.dispatchEvent(new CustomEvent('recording-stopped', {
          detail: 'composebox stopped speech query',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const dialog = app.shadowRoot.querySelector('#voiceSearchDialog');
        assertFalse(!!dialog);

        assertEquals(
            'composebox stopped speech query',
            getInputValue(composebox.$.composeboxInput.inputElement));
        assertEquals(0, testProxy.handler.getCallCount('submitQuery'));
      });

  test('voice search cancel closes dialog overlay', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    searchbox.dispatchEvent(
        new CustomEvent('open-voice-search', {bubbles: true, composed: true}));
    await microtasksFinished();

    const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
    assertTrue(!!voiceSearch);

    voiceSearch.dispatchEvent(new CustomEvent('voice-search-cancel', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    const dialog = app.shadowRoot.querySelector('#voiceSearchDialog');
    assertFalse(!!dialog);
  });

  test('voice permission changed updates CSS class', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    searchbox.dispatchEvent(
        new CustomEvent('open-voice-search', {bubbles: true, composed: true}));
    await microtasksFinished();

    const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
    assertTrue(!!voiceSearch);

    voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
      detail: {isOpened: true},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertTrue(voiceSearch.classList.contains('permission-prompt-showing'));

    voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
      detail: {isOpened: false},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertFalse(voiceSearch.classList.contains('permission-prompt-showing'));
  });

  // TODO(crbug.com/552283274): Flaky on Mac.
  // <if expr="not is_macosx">
  test(
      'open-voice-search reflects attribute on app and hides MV tiles and ' +
          'content',
      async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({
          omniboxEverywhereMostVisitedEnabled: true,
          omniboxEverywhereShowShortcuts: true,
          initialShowFre: false,
        });
        const appWithMv = document.createElement('omnibox-everywhere-app');
        document.body.appendChild(appWithMv);
        await microtasksFinished();

        const searchbox =
            appWithMv.shadowRoot.querySelector('omnibox-everywhere-omnibox');
        const mvContainer = appWithMv.shadowRoot.querySelector<HTMLElement>(
            '#mostVisitedContainer');
        const content =
            appWithMv.shadowRoot.querySelector<HTMLElement>('#content');
        assertTrue(!!searchbox);
        assertTrue(!!mvContainer);
        assertTrue(!!content);
        assertFalse(appWithMv.hasAttribute('show-voice-search-overlay_'));

        searchbox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        assertTrue(appWithMv.hasAttribute('show-voice-search-overlay_'));

        const dialog = appWithMv.shadowRoot.querySelector<HTMLDialogElement>(
            '#voiceSearchDialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);

        assertEquals('none', window.getComputedStyle(content).display);
        assertEquals(null, mvContainer.offsetParent);

        const voiceSearch = appWithMv.shadowRoot.querySelector('#voiceSearch');
        assertTrue(!!voiceSearch);
        voiceSearch.dispatchEvent(new CustomEvent('voice-search-cancel', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(appWithMv.hasAttribute('show-voice-search-overlay_'));
        assertFalse(!!appWithMv.shadowRoot.querySelector('#voiceSearchDialog'));
        assertEquals('block', window.getComputedStyle(content).display);
        assertEquals('flex', window.getComputedStyle(mvContainer).display);
      });
  // </if>

  test(
      'close-composebox event exits composebox mode and focuses searchbox',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        assertTrue(!!searchbox);

        searchbox.dispatchEvent(new CustomEvent('open-composebox', {
          detail: {text: '', files: [], mode: 0, model: 0},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);
        assertFalse(
            !!app.shadowRoot.querySelector('omnibox-everywhere-omnibox'));

        composebox.dispatchEvent(new CustomEvent('close-composebox', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(
            !!app.shadowRoot.querySelector('omnibox-everywhere-composebox'));
        const restoredSearchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox');
        assertTrue(!!restoredSearchbox);
      });

  test(
      'clicking cancel button in empty composebox closes composebox mode',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.dispatchEvent(new CustomEvent('open-composebox', {
          detail: {text: '', files: [], mode: 0, model: 0},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        const cancelIcon =
            composebox.getInputElement().shadowRoot.querySelector<HTMLElement>(
                '#cancelIcon')!;
        assertTrue(!!cancelIcon);
        cancelIcon.click();
        await microtasksFinished();

        assertFalse(
            !!app.shadowRoot.querySelector('omnibox-everywhere-composebox'));
        const restoredSearchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox');
        assertTrue(!!restoredSearchbox);
      });

  test(
      'open-voice-search in composebox reflects attribute on app and ' +
          'hides content',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox');
        assertTrue(!!searchbox);
        searchbox.dispatchEvent(new CustomEvent('open-composebox', {
          detail: {text: '', files: [], mode: 0, model: 0},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox');
        assertTrue(!!composebox);
        assertFalse(app.hasAttribute('show-voice-search-overlay_'));

        composebox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        assertTrue(app.hasAttribute('show-voice-search-overlay_'));

        const content = app.shadowRoot.querySelector('#content');
        assertTrue(!!content);
        assertEquals('none', window.getComputedStyle(content).display);

        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch');
        assertTrue(!!voiceSearch);
        voiceSearch.dispatchEvent(new CustomEvent('voice-search-cancel', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(app.hasAttribute('show-voice-search-overlay_'));
      });

  test('addFileContext Mojo event updates composebox thumbnail', async () => {
    const omniboxElement =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    const mockToken: UnguessableToken = '1234567890ABCDEF1234567890ABCDEF';

    omniboxElement.dispatchEvent(new CustomEvent('open-composebox', {
      detail: {
        text: '',
        mode: 0,
        model: 0,
        smartTabSharingActive: false,
        files: [],
      },
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    const composeboxElement =
        app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
    assertTrue(!!composeboxElement);
    assertEquals(0, composeboxElement.attachedContext.size);

    const fileInfo = {
      fileName: 'Screenshot.png',
      mimeType: 'image/png',
      imageDataUrl: 'data:image/png;base64,image_data',
      isDeletable: true,
      selectionTime: new Date(),
      thumbnailUrl: null,
    };
    testProxy.page.addFileContext(mockToken, fileInfo as SelectedFileInfo);
    await testProxy.page.$.flushForTesting();

    assertEquals(1, composeboxElement.attachedContext.size);
    const updatedFile =
        Array.from(composeboxElement.attachedContext.values())[0]!;
    assertEquals('data:image/png;base64,image_data', updatedFile.dataUrl);
  });

  test(
      'addFileContext Mojo event automatically opens composebox when in ' +
          'omnibox mode',
      async () => {
        assertFalse(
            !!app.shadowRoot.querySelector('omnibox-everywhere-composebox'));

        const mockToken: UnguessableToken = 'FEDCBA0987654321FEDCBA0987654321';
        const fileInfo = {
          fileName: 'Screenshot.png',
          mimeType: 'image/png',
          imageDataUrl: 'data:image/png;base64,image_data_buffered',
          isDeletable: true,
          selectionTime: new Date(),
          thumbnailUrl: null,
        };

        // Mojo event arrives while in standard omnibox mode.
        testProxy.page.addFileContext(mockToken, fileInfo as SelectedFileInfo);
        await testProxy.page.$.flushForTesting();
        await microtasksFinished();

        const composeboxElement =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composeboxElement);
        assertEquals(1, composeboxElement.attachedContext.size);
        const file = Array.from(composeboxElement.attachedContext.values())[0]!;
        assertEquals('data:image/png;base64,image_data_buffered', file.dataUrl);
      });
});

suite('OmniboxEverywhereProfileIconTest', () => {
  let profileIcon: OmniboxEverywhereProfileIconElement;
  let testProxy: TestSearchboxBrowserProxy;

  async function createProfileIcon(profilePickerEnabled: boolean) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: profilePickerEnabled,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    profileIcon = document.createElement('omnibox-everywhere-profile-icon');
    document.body.appendChild(profileIcon);
    await microtasksFinished();
  }

  test(
      'profile icon is not clickable or hoverable when profile picker ' +
          'is disabled',
      async () => {
        await createProfileIcon(false);
        const img =
            profileIcon.shadowRoot.querySelector<HTMLElement>('#profileIcon');
        assertTrue(!!img);
        assertFalse(img.classList.contains('clickable'));
        assertEquals('none', window.getComputedStyle(img).pointerEvents);
        assertEquals(
            'none', window.getComputedStyle(profileIcon).pointerEvents);
      });

  test(
      'profile icon is clickable and hoverable when profile picker is enabled',
      async () => {
        await createProfileIcon(true);
        const img =
            profileIcon.shadowRoot.querySelector<HTMLElement>('#profileIcon');
        assertTrue(!!img);
        assertTrue(img.classList.contains('clickable'));
        assertEquals('auto', window.getComputedStyle(img).pointerEvents);
        assertEquals('pointer', window.getComputedStyle(img).cursor);
      });
});
