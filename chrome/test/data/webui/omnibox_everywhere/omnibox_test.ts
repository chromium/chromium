// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';

import {FreChinMode} from 'chrome://omnibox-everywhere.top-chrome/fre_chin.js';
import type {FreChinElement} from 'chrome://omnibox-everywhere.top-chrome/fre_chin.js';
import {ComposeboxProxyImpl, OmniboxEverywhereBrowserProxyImpl, SearchboxBrowserProxy} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import type {OmniboxEverywhereAppElement, OmniboxEverywhereComposeboxElement, OmniboxEverywhereOmniboxElement, OmniboxEverywhereProfileIconElement} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxState} from 'chrome://resources/cr_components/composebox/common.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ContextUploadErrorType, ContextUploadStatus, InputType, ModelMode, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ContextualEntrypointButtonElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';
import {browserProxyFactory, MostVisitedPageHandlerRemote} from 'chrome://resources/cr_components/most_visited/most_visited.mojom-webui.js';
import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {SelectionDirection, SelectionLineState, SelectionStep} from 'chrome://resources/cr_components/searchbox/searchbox_selection_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {FreStage} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {TextDirection} from 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestOmniboxEverywhereBrowserProxy, TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

function getInputValue(
    inputElement: HTMLInputElement|HTMLTextAreaElement|HTMLElement): string {
  if ('value' in inputElement) {
    return (inputElement as HTMLInputElement).value;
  }
  return inputElement.innerText;
}

// `HTMLElement.click()` dispatches a click event without running the
// browser's default focus handling, so tests that emulate a click landing on
// non-interactive background have to drop focus themselves.
function blurActiveElement() {
  const activeElement = getDeepActiveElement();
  if (activeElement instanceof HTMLElement) {
    activeElement.blur();
  }
}

suite('OmniboxEverywhereOmniboxTest', () => {
  let omnibox: OmniboxEverywhereOmniboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let testEverywhereProxy: TestOmniboxEverywhereBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      isFuseboxEnabled: true,
      searchboxVoiceSearch: true,
      searchboxLensSearch: true,
      searchboxShowComposeEntrypoint: true,
      ntpRealboxDynamicAiModeButton: true,
      composeboxContextDragAndDropEnabled: true,
      energyEffectAnimationEnabled: true,
      searchboxCr23Theming: true,
      searchboxCr23SteadyStateShadow: false,
      contextManagementInComposeboxEnabled: false,
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      profileTooltipHeader: 'Chrome profile',
      omniboxEverywhereProfilePickerEnabled: false,
      isEnterpriseProfile: false,
      searchboxLayoutMode: 'TallBottomContext',
      searchboxMultiline: true,
      singleLineOnInlineAutocomplete: true,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    testEverywhereProxy = new TestOmniboxEverywhereBrowserProxy();
    OmniboxEverywhereBrowserProxyImpl.setInstance(testEverywhereProxy);
    omnibox = document.createElement('omnibox-everywhere-omnibox');
    document.body.appendChild(omnibox);
    await microtasksFinished();
  });

  test('clicking entrypoint triggers showContextActionMenu', async () => {
    const entrypoint =
        omnibox.shadowRoot.querySelector<ContextualEntrypointButtonElement>(
            '#context')!;
    assertTrue(!!entrypoint);

    entrypoint.fire('context-menu-entrypoint-click', {
      anchorRect: {x: 10, y: 20, width: 30, height: 40},
    });

    const args =
        await testEverywhereProxy.handler.whenCalled('showContextActionMenu');
    assertEquals(10, args.x);
    assertEquals(20, args.y);
    assertEquals(30, args.width);
    assertEquals(40, args.height);
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
        assertTrue(glow.energyEffectAnimationEnabled);

        const composeButton =
            omnibox.shadowRoot.querySelector('#composeButton');
        assertTrue(!!composeButton);
        assertTrue(
            composeButton.hasAttribute('energy-effect-animation-enabled'));
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

  test('executeAction clears input text and autocomplete matches', async () => {
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
              actions: [{
                hint: 'Switch to this tab',
                suggestionContents: '',
                iconPath: 'icon.png',
                a11yLabel: 'Switch to this tab',
              }],
            }),
          ],
        }));
    await microtasksFinished();
    assertEquals('query', omnibox.$.input.inputElement.value);

    const matchEl =
        omnibox.$.matches.shadowRoot?.querySelector('cr-searchbox-match');
    assertTrue(!!matchEl);
    const actionEl = matchEl.shadowRoot?.querySelector('cr-searchbox-action');
    assertTrue(!!actionEl);

    actionEl.dispatchEvent(new MouseEvent('click', {
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();

    await testProxy.handler.whenCalled('executeAction');
    assertEquals('', omnibox.$.input.inputElement.value);
    assertFalse(omnibox.dropdownIsVisible);
  });

  test('multiLineEnabled is initialized from loadTimeData', () => {
    assertTrue(omnibox.multiLineEnabled);
    assertTrue(omnibox.hasAttribute('multi-line-enabled'));
  });

  test(
      'updateDropdownVisibility suppresses dropdown when multiline input ' +
          'expands',
      () => {
        omnibox.result = createAutocompleteResultForTesting({
          input: 'multiline text',
          matches: [createSearchMatchForTesting()],
        });
        omnibox.dropdownIsVisible = true;

        Object.defineProperty(omnibox.$.input.inputElement, 'scrollHeight', {
          value: 64,
          configurable: true,
        });

        omnibox.updateDropdownVisibility();
        assertFalse(omnibox.dropdownIsVisible);
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
      'updateAimPopupEligibility updates dynamically when initially disabled',
      async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({
          isFuseboxEnabled: false,
          searchboxShowComposeEntrypoint: false,
          searchboxVoiceSearch: true,
          searchboxLensSearch: true,
        });
        const testOmnibox =
            document.createElement('omnibox-everywhere-omnibox');
        document.body.appendChild(testOmnibox);
        await microtasksFinished();

        assertFalse(!!testOmnibox.shadowRoot.querySelector('#composeButton'));
        assertFalse(!!testOmnibox.shadowRoot.querySelector('#context'));
        assertFalse(
            !!testOmnibox.shadowRoot.querySelector('#lensSearchButton'));

        testProxy.page.updateAimPopupEligibility(true);
        await microtasksFinished();

        assertTrue(!!testOmnibox.shadowRoot.querySelector('#composeButton'));
        assertTrue(!!testOmnibox.shadowRoot.querySelector('#context'));
        assertTrue(!!testOmnibox.shadowRoot.querySelector('#lensSearchButton'));

        testProxy.page.updateAimPopupEligibility(false);
        await microtasksFinished();

        assertFalse(!!testOmnibox.shadowRoot.querySelector('#composeButton'));
        assertFalse(!!testOmnibox.shadowRoot.querySelector('#context'));
        assertFalse(
            !!testOmnibox.shadowRoot.querySelector('#lensSearchButton'));
      });
  test('dropdownIsVisible preserves bottomControls', async () => {
    const bottomControls =
        omnibox.shadowRoot.querySelector<HTMLElement>('#bottomControls');
    assertTrue(!!bottomControls);
    assertFalse(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);
    assertTrue(!!omnibox.shadowRoot.querySelector('#voiceSearchButton'));
    assertTrue(!!omnibox.shadowRoot.querySelector('#lensSearchButton'));

    omnibox.dropdownIsVisible = true;
    await microtasksFinished();

    assertTrue(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);
    assertTrue(!!omnibox.shadowRoot.querySelector('#voiceSearchButton'));
    assertTrue(!!omnibox.shadowRoot.querySelector('#lensSearchButton'));

    omnibox.dropdownIsVisible = false;
    await microtasksFinished();

    assertFalse(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);
    assertTrue(!!omnibox.shadowRoot.querySelector('#voiceSearchButton'));
    assertTrue(!!omnibox.shadowRoot.querySelector('#lensSearchButton'));
  });

  test(
      'voice and lens search buttons remain visible with user input',
      async () => {
        assertTrue(!!omnibox.shadowRoot.querySelector('#voiceSearchButton'));
        assertTrue(!!omnibox.shadowRoot.querySelector('#lensSearchButton'));

        omnibox.setInputText('hello world');
        await microtasksFinished();

        assertTrue(!!omnibox.shadowRoot.querySelector('#voiceSearchButton'));
        assertTrue(!!omnibox.shadowRoot.querySelector('#lensSearchButton'));

        omnibox.dropdownIsVisible = true;
        await microtasksFinished();

        assertTrue(!!omnibox.shadowRoot.querySelector('#voiceSearchButton'));
        assertTrue(!!omnibox.shadowRoot.querySelector('#lensSearchButton'));

        omnibox.setInputText('');
        await microtasksFinished();

        assertTrue(!!omnibox.shadowRoot.querySelector('#voiceSearchButton'));
        assertTrue(!!omnibox.shadowRoot.querySelector('#lensSearchButton'));
      });

  test('balanced layout and element clearances', () => {
    const inputWrapper =
        omnibox.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!inputWrapper);
    const wrapperStyle = window.getComputedStyle(inputWrapper);
    assertEquals('16px', wrapperStyle.borderRadius);
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
    assertEquals('36px', window.getComputedStyle(composeButton).marginInlineEnd);

    const profileIcon =
        omnibox.shadowRoot.querySelector<HTMLElement>('#profileIcon');
    assertTrue(!!profileIcon);
    assertEquals('16px', window.getComputedStyle(profileIcon).top);
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

  test('clicking lens button hides lens IPH help bubble', async () => {
    let hideCalledWith = '';
    const originalHideHelpBubble = omnibox.hideHelpBubble.bind(omnibox);
    omnibox.hideHelpBubble = (nativeId: string) => {
      hideCalledWith = nativeId;
      return originalHideHelpBubble(nativeId);
    };

    const lensButton =
        omnibox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
    assertTrue(!!lensButton);

    lensButton.click();
    await microtasksFinished();

    assertEquals('kOmniboxEverywhereLensButtonElementId', hideCalledWith);
  });

  test(
      'clicking lens button again toggles it off without calling ' +
          'showScreenshotMenu again',
      async () => {
        const lensButton =
            omnibox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
        assertTrue(!!lensButton);
        assertFalse(omnibox.isScreenshotMenuOpen);

        lensButton.click();
        await microtasksFinished();

        assertTrue(omnibox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));

        lensButton.click();
        await microtasksFinished();

        assertFalse(omnibox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));
      });

  test(
      'pointerdown on lens button while open suppresses reopening on click ' +
          'even after onScreenshotMenuClosed',
      async () => {
        const lensButton =
            omnibox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
        assertTrue(!!lensButton);

        lensButton.click();
        await microtasksFinished();
        assertTrue(omnibox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));

        lensButton.dispatchEvent(new PointerEvent('pointerdown', {
          bubbles: true,
          composed: true,
          button: 0,
        }));
        testProxy.page.onScreenshotMenuClosed();
        await microtasksFinished();
        assertFalse(omnibox.isScreenshotMenuOpen);

        lensButton.click();
        await microtasksFinished();

        assertFalse(omnibox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));
      });

  test(
      'non-primary pointerdown on lens button does not suppress reopening',
      async () => {
        const lensButton =
            omnibox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
        assertTrue(!!lensButton);

        lensButton.click();
        await microtasksFinished();
        assertTrue(omnibox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));

        lensButton.dispatchEvent(new PointerEvent('pointerdown', {
          bubbles: true,
          composed: true,
          button: 2,
        }));
        testProxy.page.onScreenshotMenuClosed();
        await microtasksFinished();
        assertFalse(omnibox.isScreenshotMenuOpen);

        lensButton.click();
        await microtasksFinished();

        assertTrue(omnibox.isScreenshotMenuOpen);
        assertEquals(2, testProxy.handler.getCallCount('showScreenshotMenu'));
      });

  test('pointercancel on lens button resets suppression', async () => {
    const lensButton =
        omnibox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
    assertTrue(!!lensButton);

    lensButton.click();
    await microtasksFinished();
    assertTrue(omnibox.isScreenshotMenuOpen);
    assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));

    lensButton.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      composed: true,
      button: 0,
    }));
    lensButton.dispatchEvent(new PointerEvent('pointercancel', {
      bubbles: true,
      composed: true,
    }));
    testProxy.page.onScreenshotMenuClosed();
    await microtasksFinished();
    assertFalse(omnibox.isScreenshotMenuOpen);

    lensButton.click();
    await microtasksFinished();

    assertTrue(omnibox.isScreenshotMenuOpen);
    assertEquals(2, testProxy.handler.getCallCount('showScreenshotMenu'));
  });

  test(
      'stepCyclesSelection returns false to cycle within popup like Omnibox',
      () => {
        const match = createSearchMatchForTesting();
        const result = createAutocompleteResultForTesting({matches: [match]});
        const selection = {
          line: 0,
          state: SelectionLineState.kNormal,
          actionIndex: 0,
        };

        assertFalse(omnibox.stepCyclesSelection(
            result, selection, SelectionDirection.kForward,
            SelectionStep.kStateOrLine));
        assertFalse(omnibox.stepCyclesSelection(
            result, selection, SelectionDirection.kBackward,
            SelectionStep.kStateOrLine));
      });

  test('showContextEntrypoint reflects isFuseboxEnabled', async () => {
    assertTrue(omnibox.showContextEntrypoint);

    testProxy.page.updateAimPopupEligibility(false);
    await microtasksFinished();

    assertFalse(omnibox.showContextEntrypoint);
  });

  test(
      'Tab and Shift+Tab navigate across AIM, contextual entrypoint (+), ' +
          'voice search, lens search, and back to input when dropdown is ' +
          'visible',
      async () => {
        omnibox.virtualFocusEnabled = true;
        omnibox.dropdownIsVisible = true;
        const match =
            createSearchMatchForTesting({allowedToBeDefaultMatch: true});
        const result = createAutocompleteResultForTesting({matches: [match]});
        omnibox.activeQueryId = 0;
        testProxy.page.autocompleteResultChanged(result);
        await microtasksFinished();

        assertEquals(0, omnibox.selection.line);
        assertEquals(SelectionLineState.kNormal, omnibox.selection.state);

        const entrypoint =
            omnibox.shadowRoot.querySelector<ContextualEntrypointButtonElement>(
                '#context')!;
        const voiceContainer = omnibox.shadowRoot.querySelector<HTMLElement>(
            '.searchbox-icon-button-container.voice')!;
        const lensContainer = omnibox.shadowRoot.querySelector<HTMLElement>(
            '.searchbox-icon-button-container.lens')!;

        const dispatchTab = async (shiftKey: boolean = false) => {
          const tabEvent = new KeyboardEvent('keydown', {
            key: 'Tab',
            shiftKey,
            bubbles: true,
            composed: true,
            cancelable: true,
          });
          omnibox.$.input.inputElement.dispatchEvent(tabEvent);
          await microtasksFinished();
          assertTrue(tabEvent.defaultPrevented);
        };

        // Tab 1 -> AIM button.
        await dispatchTab();
        assertEquals(
            SelectionLineState.kFocusedButtonAim, omnibox.selection.state);

        // Tab 2 -> Contextual entrypoint (+) button.
        await dispatchTab();
        assertEquals(
            SelectionLineState.kFocusedButtonContextEntrypoint,
            omnibox.selection.state);
        assertTrue(entrypoint.hasVirtualFocus);
        assertFalse(voiceContainer.hasAttribute('has-virtual-focus'));
        assertFalse(lensContainer.hasAttribute('has-virtual-focus'));

        // Tab 3 -> Voice search (Mic) button.
        await dispatchTab();
        assertEquals(
            SelectionLineState.kFocusedButtonVoiceSearch,
            omnibox.selection.state);
        assertFalse(entrypoint.hasVirtualFocus);
        assertTrue(voiceContainer.hasAttribute('has-virtual-focus'));
        assertFalse(lensContainer.hasAttribute('has-virtual-focus'));

        // Tab 4 -> Lens search button.
        await dispatchTab();
        assertEquals(
            SelectionLineState.kFocusedButtonLensSearch,
            omnibox.selection.state);
        assertFalse(entrypoint.hasVirtualFocus);
        assertFalse(voiceContainer.hasAttribute('has-virtual-focus'));
        assertTrue(lensContainer.hasAttribute('has-virtual-focus'));

        // Tab 5 -> Wraps back to default match / search input (line 0).
        await dispatchTab();
        assertEquals(0, omnibox.selection.line);
        assertEquals(SelectionLineState.kNormal, omnibox.selection.state);
        assertFalse(lensContainer.hasAttribute('has-virtual-focus'));

        // Shift+Tab 1 -> Back to Lens search button.
        await dispatchTab(/*shiftKey=*/ true);
        assertEquals(
            SelectionLineState.kFocusedButtonLensSearch,
            omnibox.selection.state);
        assertTrue(lensContainer.hasAttribute('has-virtual-focus'));

        // Shift+Tab 2 -> Back to Voice search (Mic) button.
        await dispatchTab(/*shiftKey=*/ true);
        assertEquals(
            SelectionLineState.kFocusedButtonVoiceSearch,
            omnibox.selection.state);
        assertTrue(voiceContainer.hasAttribute('has-virtual-focus'));

        // Shift+Tab 3 -> Back to Contextual entrypoint (+) button.
        await dispatchTab(/*shiftKey=*/ true);
        assertEquals(
            SelectionLineState.kFocusedButtonContextEntrypoint,
            omnibox.selection.state);
        assertTrue(entrypoint.hasVirtualFocus);
      });

  test(
      'Enter on virtually focused contextual entrypoint triggers ' +
          'showContextActionMenu',
      async () => {
        omnibox.virtualFocusEnabled = true;
        omnibox.dropdownIsVisible = true;
        const match = createSearchMatchForTesting();
        omnibox.activeQueryId = 0;
        testProxy.page.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              queryId: 0,
              input: 'query',
              matches: [match],
            }));
        await microtasksFinished();

        omnibox.setSelection({
          line: -1,
          state: SelectionLineState.kFocusedButtonContextEntrypoint,
          actionIndex: 0,
        });
        await microtasksFinished();

        const enterEvent = new KeyboardEvent('keydown', {
          key: 'Enter',
          bubbles: true,
          composed: true,
          cancelable: true,
        });
        omnibox.$.input.inputElement.dispatchEvent(enterEvent);
        await microtasksFinished();

        assertTrue(enterEvent.defaultPrevented);
        const args = await testEverywhereProxy.handler.whenCalled(
            'showContextActionMenu');
        assertTrue(args !== undefined);
      });

  test(
      'singleLineOnInlineAutocomplete keeps input single line ' +
          'with inline autocompletion',
      async () => {
        omnibox.multiLineEnabled = true;
        await omnibox.updateComplete;
        await omnibox.$.input.updateComplete;

        assertTrue(omnibox.singleLineOnInlineAutocomplete);
        assertTrue(omnibox.$.input.singleLineOnInlineAutocomplete);

        omnibox.$.input.setInput({text: 'm', inline: 'essages.google.com'});
        await omnibox.$.input.updateComplete;

        assertTrue(omnibox.$.input.hasAttribute('force-single-line'));
        assertFalse(omnibox.$.input.isMultiline());

        omnibox.result = createAutocompleteResultForTesting({
          input: 'm',
          matches: [createSearchMatchForTesting({
            allowedToBeDefaultMatch: true,
            inlineAutocompletion: 'essages.google.com',
          })],
        });
        omnibox.dropdownIsVisible = true;

        omnibox.updateDropdownVisibility();
        assertTrue(omnibox.dropdownIsVisible);
      });

  test(
      'singleLineAutocomplete preserves single line and dropdown on arrow down',
      async () => {
        assertTrue(omnibox.singleLineOnInlineAutocomplete);
        assertTrue(omnibox.$.input.singleLineOnInlineAutocomplete);

        omnibox.activeQueryId = 0;
        omnibox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: 0,
          input: 'query',
          matches: [
            createSearchMatchForTesting({
              allowedToBeDefaultMatch: true,
              fillIntoEdit: 'query',
            }),
            createSearchMatchForTesting({
              allowedToBeDefaultMatch: false,
              fillIntoEdit: 'query with a very long second suggestion text',
            }),
          ],
        }));
        await microtasksFinished();

        omnibox.$.input.inputElement.focus();
        const arrowDownEvent = new KeyboardEvent('keydown', {
          key: 'ArrowDown',
          bubbles: true,
          cancelable: true,
          composed: true,
        });
        omnibox.$.input.inputElement.dispatchEvent(arrowDownEvent);
        await microtasksFinished();
        await omnibox.updateComplete;
        await omnibox.$.input.updateComplete;

        assertTrue(omnibox.$.input.hasAttribute('force-single-line'));
        assertFalse(omnibox.$.input.isMultiline());
        assertTrue(omnibox.dropdownIsVisible);
      });

  test(
      'Enter on virtually focused voice and lens buttons activates them',
      async () => {
        omnibox.virtualFocusEnabled = true;
        omnibox.dropdownIsVisible = true;
        const match = createSearchMatchForTesting();
        omnibox.activeQueryId = 0;
        testProxy.page.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              queryId: 0,
              input: 'query',
              matches: [match],
            }));
        await microtasksFinished();

        let voiceSearchDispatched = false;
        omnibox.addEventListener('open-voice-search', () => {
          voiceSearchDispatched = true;
        });

        omnibox.setSelection({
          line: -1,
          state: SelectionLineState.kFocusedButtonVoiceSearch,
          actionIndex: 0,
        });
        await microtasksFinished();

        const enterVoiceEvent = new KeyboardEvent('keydown', {
          key: 'Enter',
          bubbles: true,
          composed: true,
          cancelable: true,
        });
        omnibox.$.input.inputElement.dispatchEvent(enterVoiceEvent);
        await microtasksFinished();

        assertTrue(enterVoiceEvent.defaultPrevented);
        assertTrue(voiceSearchDispatched);

        omnibox.setSelection({
          line: -1,
          state: SelectionLineState.kFocusedButtonLensSearch,
          actionIndex: 0,
        });
        await microtasksFinished();

        const enterLensEvent = new KeyboardEvent('keydown', {
          key: 'Enter',
          bubbles: true,
          composed: true,
          cancelable: true,
        });
        omnibox.$.input.inputElement.dispatchEvent(enterLensEvent);
        await microtasksFinished();

        assertTrue(enterLensEvent.defaultPrevented);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));
      });
});


suite('OmniboxEverywhereComposeboxTest', () => {
  let composebox: OmniboxEverywhereComposeboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let testEverywhereProxy: TestOmniboxEverywhereBrowserProxy;
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
    testEverywhereProxy = new TestOmniboxEverywhereBrowserProxy();
    OmniboxEverywhereBrowserProxyImpl.setInstance(testEverywhereProxy);
    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockPageHandler,
        testProxy.handler as unknown as SearchboxPageHandlerRemote,
        testProxy.callbackRouter as unknown as SearchboxPageCallbackRouter));

    composebox = document.createElement('omnibox-everywhere-composebox');
    document.body.appendChild(composebox);
    await microtasksFinished();
  });

  test('configures animated glow on composebox correctly', async () => {
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();
    const glow = composebox.shadowRoot.querySelector<SearchAnimatedGlowElement>(
        '#animatedSearchElement');
    assertTrue(!!glow);
    assertEquals('OmniboxEverywhere', glow.entrypointName);
    assertEquals('expanding', glow.animationState);
    assertTrue(glow.energyEffectAnimationEnabled);
  });

  test(
      'playGlowAnimation resets animation state to NONE after timeout',
      async () => {
        composebox.playGlowAnimation(/*timeoutMs=*/ 10);
        assertEquals(GlowAnimationState.NONE, composebox.animationState);

        await new Promise(resolve => requestAnimationFrame(resolve));
        await microtasksFinished();

        // After rAF callback runs, animationState is EXPANDING.
        assertEquals(GlowAnimationState.EXPANDING, composebox.animationState);

        // After timeoutMs elapses, animationState resets to NONE.
        await new Promise(r => setTimeout(r, 20));
        await microtasksFinished();

        assertEquals(GlowAnimationState.NONE, composebox.animationState);
      });

  test(
      'disconnectedCallback cancels pending glow animation rAF and timeout',
      async () => {
        const testElem =
            document.createElement('omnibox-everywhere-composebox');
        document.body.appendChild(testElem);
        await microtasksFinished();

        testElem.playGlowAnimation(/*timeoutMs=*/ 50);
        assertEquals(GlowAnimationState.NONE, testElem.animationState);

        // Disconnect immediately before the next animation frame executes.
        testElem.remove();

        await new Promise(r => setTimeout(r, 60));
        await microtasksFinished();

        // Animation should not have transitioned to EXPANDING.
        assertEquals(GlowAnimationState.NONE, testElem.animationState);
      });

  test(
      'clicking contextEntrypoint triggers showContextActionMenu', async () => {
        const entrypoint =
            composebox.shadowRoot
                .querySelector<ContextualEntrypointButtonElement>(
                    '#contextEntrypoint')!;
        assertTrue(!!entrypoint);

        entrypoint.fire('context-menu-entrypoint-click', {
          anchorRect: {x: 15, y: 25, width: 35, height: 45},
        });

        const args = await testEverywhereProxy.handler.whenCalled(
            'showContextActionMenu');
        assertEquals(15, args.x);
        assertEquals(25, args.y);
        assertEquals(35, args.width);
        assertEquals(45, args.height);
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

  test('computeShowDropdown returns true when context menu is open', () => {
    composebox.isContextMenuOpen = true;
    assertTrue(composebox.computeShowDropdown());

    composebox.isContextMenuOpen = false;
    composebox.showDropdown = false;
    assertFalse(composebox.computeShowDropdown());
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

  test(
      'clicking lens button in composebox hides lens IPH help bubble',
      async () => {
        let hideCalledWith = '';
        const originalHideHelpBubble =
            composebox.hideHelpBubble.bind(composebox);
        composebox.hideHelpBubble = (nativeId: string) => {
          hideCalledWith = nativeId;
          return originalHideHelpBubble(nativeId);
        };

        const lensButton = composebox.shadowRoot.querySelector<HTMLElement>(
            '#lensSearchButton');
        assertTrue(!!lensButton);

        lensButton.click();
        await microtasksFinished();

        assertEquals('kOmniboxEverywhereLensButtonElementId', hideCalledWith);
      });

  test(
      'clicking lens button in composebox again toggles it off without ' +
          'calling showScreenshotMenu again',
      async () => {
        const lensButton = composebox.shadowRoot.querySelector<HTMLElement>(
            '#lensSearchButton');
        assertTrue(!!lensButton);
        assertFalse(composebox.isScreenshotMenuOpen);

        lensButton.click();
        await microtasksFinished();

        assertTrue(composebox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));

        lensButton.click();
        await microtasksFinished();

        assertFalse(composebox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));
      });

  test(
      'pointerdown on composebox lens button while open suppresses ' +
          'reopening on click even after onScreenshotMenuClosed',
      async () => {
        const lensButton = composebox.shadowRoot.querySelector<HTMLElement>(
            '#lensSearchButton');
        assertTrue(!!lensButton);

        lensButton.click();
        await microtasksFinished();
        assertTrue(composebox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));

        lensButton.dispatchEvent(new PointerEvent('pointerdown', {
          bubbles: true,
          composed: true,
          button: 0,
        }));
        testProxy.page.onScreenshotMenuClosed();
        await microtasksFinished();
        assertFalse(composebox.isScreenshotMenuOpen);

        lensButton.click();
        await microtasksFinished();

        assertFalse(composebox.isScreenshotMenuOpen);
        assertEquals(1, testProxy.handler.getCallCount('showScreenshotMenu'));
      });

  test('deleting file context queries autocomplete', async () => {
    const token = 'test-token';
    const file = ComposeboxFile.createFromFile(
        token, {name: 'file.png', type: 'image/png'});
    composebox.onFileContextAdded(file);
    await microtasksFinished();

    testProxy.handler.resetResolver('queryAutocomplete');
    composebox.deleteFile(token, /*fromUserAction=*/ true);
    await microtasksFinished();

    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const args = testProxy.handler.getArgs('queryAutocomplete')[0];
    assertEquals('', args[2]);
    assertFalse(args[3]);
    assertEquals(null, composebox.result);
  });

  test('context upload replaced queries autocomplete', async () => {
    const token = 'test-token';
    const file = ComposeboxFile.createFromFile(
        token, {name: 'file.png', type: 'image/png'});
    composebox.onFileContextAdded(file);
    await microtasksFinished();

    testProxy.handler.resetResolver('queryAutocomplete');
    composebox.onContextualInputStatusChanged(
        token, ContextUploadStatus.kUploadReplaced, null);
    await microtasksFinished();

    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const args = testProxy.handler.getArgs('queryAutocomplete')[0];
    assertEquals('', args[2]);
    assertFalse(args[3]);
    assertEquals(null, composebox.result);
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
  let testEverywhereProxy: TestOmniboxEverywhereBrowserProxy;
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
      smallLoomnibox: true,
      isPersistentMode: true,
      omniboxEverywhereMostVisitedHideTitle: true,
      initialFreStage: 0,
      composeboxCancelButtonTitle: 'Close AI Mode',
      composeboxCancelButtonTitleInput: 'Clear text',
    });

    const mostVisitedHandler = TestMock.fromClass(MostVisitedPageHandlerRemote);
    const {instance: mostVisitedInstance} =
        browserProxyFactory.createForTest(mostVisitedHandler);
    browserProxyFactory.setInstance(mostVisitedInstance);
    mostVisitedHandler.setResultFor(
        'getMostVisitedExpandedState', Promise.resolve({isExpanded: false}));

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    testEverywhereProxy = new TestOmniboxEverywhereBrowserProxy();
    OmniboxEverywhereBrowserProxyImpl.setInstance(testEverywhereProxy);
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
    searchbox.fire('open-voice-search');
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
        assertTrue(app.classList.contains('has-permission-prompt'));
        assertEquals(
            '200px',
            app.style.getPropertyValue('--voice_search_minimum_height'));
        assertEquals(
            '100px',
            app.style.getPropertyValue('--voice_search_minimum_width'));

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
        assertFalse(app.classList.contains('has-permission-prompt'));
        assertEquals(
            '', app.style.getPropertyValue('--voice_search_minimum_height'));
        assertEquals(
            '', app.style.getPropertyValue('--voice_search_minimum_width'));
      });

  test(
      'clicking voice search button in composebox opens voice search dialog' +
          ' overlay and handles permission prompt',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire('open-composebox', {
          text: '',
          files: [],
          mode: 0,
          model: 0,
        });
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
        assertTrue(app.classList.contains('has-permission-prompt'));
        assertEquals(
            '200px',
            app.style.getPropertyValue('--voice_search_minimum_height'));
        assertEquals(
            '100px',
            app.style.getPropertyValue('--voice_search_minimum_width'));

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
        assertFalse(app.classList.contains('has-permission-prompt'));
        assertEquals(
            '', app.style.getPropertyValue('--voice_search_minimum_height'));
        assertEquals(
            '', app.style.getPropertyValue('--voice_search_minimum_width'));
      });

  test(
      'voice search final result submits query and closes dialog', async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire('open-voice-search');
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
    searchbox.fire(
        'open-composebox',
        {text: 'searchbox text', files: [], mode: 0, model: 0});
    await microtasksFinished();

    const composebox =
        app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
    assertTrue(!!composebox);

    composebox.fire('composebox-submit');
    await microtasksFinished();

    assertFalse(
        !!app.shadowRoot.querySelector('omnibox-everywhere-composebox'));
    const restoredSearchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    assertTrue(!!restoredSearchbox);
    assertEquals('', restoredSearchbox.$.input.inputElement.value);
  });

  test(
      'disabling aim popup eligibility closes active composebox mode',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire(
            'open-composebox',
            {text: 'draft text', files: [], mode: 0, model: 0});
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        testProxy.page.updateAimPopupEligibility(false);
        await microtasksFinished();

        assertFalse(
            !!app.shadowRoot.querySelector('omnibox-everywhere-composebox'));
        const restoredSearchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        assertTrue(!!restoredSearchbox);
      });

  test(
      'mode transitions call setIsComposebox on browser proxy handler',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
        await microtasksFinished();

        let isComposebox =
            await testEverywhereProxy.handler.whenCalled('setIsComposebox');
        assertTrue(isComposebox);
        testEverywhereProxy.handler.reset();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        composebox.fire('close-composebox');
        await microtasksFinished();

        isComposebox =
            await testEverywhereProxy.handler.whenCalled('setIsComposebox');
        assertFalse(isComposebox);
        testEverywhereProxy.handler.reset();

        // Reopen composebox and verify query submission cleans up UI without
        // calling setIsComposebox across Mojo.
        searchbox.fire(
            'open-composebox', {text: 'query', files: [], mode: 0, model: 0});
        await microtasksFinished();
        isComposebox =
            await testEverywhereProxy.handler.whenCalled('setIsComposebox');
        assertTrue(isComposebox);
        testEverywhereProxy.handler.reset();

        const activeComposebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!activeComposebox);
        activeComposebox.fire('composebox-submit');
        await microtasksFinished();

        assertEquals(
            0, testEverywhereProxy.handler.getCallCount('setIsComposebox'));
        const restoredSearchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox');
        assertTrue(!!restoredSearchbox);
      });

  test(
      'voice search final result in composebox submits query and closes dialog',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        composebox.fire('open-voice-search');
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
        searchbox.fire('open-voice-search');
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
        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        composebox.fire('open-voice-search');
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
    searchbox.fire('open-voice-search');
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

  test(
      'voice permission changed updates CSS class and dimensions', async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire('open-voice-search');
        await microtasksFinished();

        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);
        const glow = app.shadowRoot.querySelector<SearchAnimatedGlowElement>(
            '#voiceSearchGlow');
        assertTrue(!!glow);

        // Initial state: no prompt class or minimum dimensions set on host.
        assertFalse(app.classList.contains('has-permission-prompt'));
        assertEquals(
            '', app.style.getPropertyValue('--voice_search_minimum_height'));
        assertEquals(
            '', app.style.getPropertyValue('--voice_search_minimum_width'));

        voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: true,
            height: 120,
            width: 250,
          },
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(voiceSearch.classList.contains('permission-prompt-showing'));
        assertTrue(glow.classList.contains('permission-prompt-showing'));
        assertTrue(app.classList.contains('has-permission-prompt'));
        assertEquals(
            '120px',
            app.style.getPropertyValue('--voice_search_minimum_height'));
        assertEquals(
            '250px',
            app.style.getPropertyValue('--voice_search_minimum_width'));

        voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: false,
            height: 0,
            width: 0,
          },
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(
            voiceSearch.classList.contains('permission-prompt-showing'));
        assertFalse(glow.classList.contains('permission-prompt-showing'));
        assertFalse(app.classList.contains('has-permission-prompt'));
        assertEquals(
            '', app.style.getPropertyValue('--voice_search_minimum_height'));
        assertEquals(
            '', app.style.getPropertyValue('--voice_search_minimum_width'));
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
          initialFreStage: 0,
        });
        const mvHandler = TestMock.fromClass(MostVisitedPageHandlerRemote);
        const {instance: mvInstance, remote: mvRemote} =
            browserProxyFactory.createForTest(mvHandler);
        browserProxyFactory.setInstance(mvInstance);
        mvHandler.setResultFor(
            'getMostVisitedExpandedState',
            Promise.resolve({isExpanded: false}));

        const appWithMv = document.createElement('omnibox-everywhere-app');
        document.body.appendChild(appWithMv);
        await microtasksFinished();

        const testTiles = [{
          title: 'Google',
          titleDirection: TextDirection.LEFT_TO_RIGHT,
          url: 'https://www.google.com/',
          source: 0,
          titleSource: 0,
          isQueryTile: false,
          allowUserEdit: false,
          allowUserDelete: false,
        }];
        mvRemote.setMostVisitedInfo({
          customLinksEnabled: false,
          enterpriseShortcutsEnabled: false,
          tiles: testTiles,
          visible: true,
        });
        await mvRemote.$.flushForTesting();
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

        searchbox.fire('open-voice-search');
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
      'MVT container hides when context menu, screenshot menu, or user input is present',
      async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({
          omniboxEverywhereMostVisitedEnabled: true,
          initialFreStage: 0,
        });
        const mvHandler = TestMock.fromClass(MostVisitedPageHandlerRemote);
        const {instance: mvInstance, remote: mvRemote} =
            browserProxyFactory.createForTest(mvHandler);
        browserProxyFactory.setInstance(mvInstance);
        mvHandler.setResultFor(
            'getMostVisitedExpandedState',
            Promise.resolve({isExpanded: false}));

        const appWithMv = document.createElement('omnibox-everywhere-app');
        document.body.appendChild(appWithMv);
        await microtasksFinished();

        const testTiles = [{
          title: 'Google',
          titleDirection: TextDirection.LEFT_TO_RIGHT,
          url: 'https://www.google.com/',
          source: 0,
          titleSource: 0,
          isQueryTile: false,
          allowUserEdit: false,
          allowUserDelete: false,
        }];
        mvRemote.setMostVisitedInfo({
          customLinksEnabled: false,
          enterpriseShortcutsEnabled: false,
          tiles: testTiles,
          visible: true,
        });
        await mvRemote.$.flushForTesting();
        await microtasksFinished();

        const searchbox =
            appWithMv.shadowRoot.querySelector('omnibox-everywhere-omnibox');
        const mvContainer = appWithMv.shadowRoot.querySelector<HTMLElement>(
            '#mostVisitedContainer');
        assertTrue(!!searchbox);
        assertTrue(!!mvContainer);
        assertEquals('flex', window.getComputedStyle(mvContainer).display);

        // Opening context (plus) menu hides MVT container.
        searchbox.isContextMenuOpen = true;
        await microtasksFinished();
        assertEquals('none', window.getComputedStyle(mvContainer).display);

        // Closing context menu restores MVT container.
        searchbox.isContextMenuOpen = false;
        await microtasksFinished();
        assertEquals('flex', window.getComputedStyle(mvContainer).display);

        // Opening screenshot (lens) menu hides MVT container.
        searchbox.isScreenshotMenuOpen = true;
        await microtasksFinished();
        assertEquals('none', window.getComputedStyle(mvContainer).display);

        // Closing screenshot menu restores MVT container.
        searchbox.isScreenshotMenuOpen = false;
        await microtasksFinished();
        assertEquals('flex', window.getComputedStyle(mvContainer).display);

        // Having user input hides MVT container even if dropdown is not
        // visible.
        searchbox.setInputText('query');
        await microtasksFinished();
        assertFalse(searchbox.dropdownIsVisible);
        assertEquals('none', window.getComputedStyle(mvContainer).display);

        // Clearing input text restores MVT container.
        searchbox.setInputText('');
        await microtasksFinished();
        assertEquals('flex', window.getComputedStyle(mvContainer).display);
      });

  test('dynamic MVT visibility updates container visibility', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      omniboxEverywhereMostVisitedEnabled: true,
      initialFreStage: 0,
    });
    const mvHandler = TestMock.fromClass(MostVisitedPageHandlerRemote);
    const {instance: mvInstance, remote: mvRemote} =
        browserProxyFactory.createForTest(mvHandler);
    browserProxyFactory.setInstance(mvInstance);
    mvHandler.setResultFor(
        'getMostVisitedExpandedState', Promise.resolve({isExpanded: false}));

    const appWithMv = document.createElement('omnibox-everywhere-app');
    document.body.appendChild(appWithMv);
    await microtasksFinished();

    const mvContainer = appWithMv.shadowRoot.querySelector<HTMLElement>(
        '#mostVisitedContainer');
    const mostVisited = appWithMv.shadowRoot.querySelector('cr-most-visited');
    assertTrue(!!mvContainer);
    assertTrue(!!mostVisited);
    assertTrue(mvContainer.hidden);

    const testTiles = [{
      title: 'Google',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: 'https://www.google.com/',
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      allowUserEdit: false,
      allowUserDelete: false,
    }];

    mvRemote.setMostVisitedInfo({
      customLinksEnabled: false,
      enterpriseShortcutsEnabled: false,
      tiles: testTiles,
      visible: true,
    });
    await mvRemote.$.flushForTesting();
    await microtasksFinished();

    assertFalse(mvContainer.hidden);

    mvRemote.setMostVisitedInfo({
      customLinksEnabled: false,
      enterpriseShortcutsEnabled: false,
      tiles: testTiles,
      visible: false,
    });
    await mvRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(mvContainer.hidden);
  });

  test(
      'MVT remains mounted and hidden during FRE, unhiding when FRE dismissed',
      async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({
          omniboxEverywhereMostVisitedEnabled: true,
          initialFreStage: FreStage.kIntroModal,
        });
        const mvHandler = TestMock.fromClass(MostVisitedPageHandlerRemote);
        const {instance: mvInstance, remote: mvRemote} =
            browserProxyFactory.createForTest(mvHandler);
        browserProxyFactory.setInstance(mvInstance);
        mvHandler.setResultFor(
            'getMostVisitedExpandedState',
            Promise.resolve({isExpanded: false}));

        const appWithMv = document.createElement('omnibox-everywhere-app');
        document.body.appendChild(appWithMv);
        await microtasksFinished();

        const mvContainer = appWithMv.shadowRoot.querySelector<HTMLElement>(
            '#mostVisitedContainer');
        const mostVisited =
            appWithMv.shadowRoot.querySelector('cr-most-visited');
        assertTrue(!!mvContainer);
        assertTrue(!!mostVisited);
        assertTrue(mvContainer.hidden);

        const testTiles = [{
          title: 'Google',
          titleDirection: TextDirection.LEFT_TO_RIGHT,
          url: 'https://www.google.com/',
          source: 0,
          titleSource: 0,
          isQueryTile: false,
          allowUserEdit: false,
          allowUserDelete: false,
        }];

        mvRemote.setMostVisitedInfo({
          customLinksEnabled: false,
          enterpriseShortcutsEnabled: false,
          tiles: testTiles,
          visible: true,
        });
        await mvRemote.$.flushForTesting();
        await microtasksFinished();

        // Still hidden because FRE modal is active.
        assertTrue(mvContainer.hidden);

        // Transition FRE to kNone.
        testProxy.page.setFreState({
          stage: FreStage.kNone,
          currentHotkeyTokens: [],
        });
        await microtasksFinished();

        // Now unhidden.
        assertFalse(mvContainer.hidden);
      });

  test('smallLoomnibox controls max-tiles and attribute', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      omniboxEverywhereMostVisitedEnabled: true,
      smallLoomnibox: true,
      initialFreStage: 0,
    });
    const smallApp = document.createElement('omnibox-everywhere-app');
    document.body.appendChild(smallApp);
    await microtasksFinished();

    assertTrue(smallApp.hasAttribute('small-loomnibox'));
    const mv = smallApp.shadowRoot.querySelector('cr-most-visited')!;
    assertTrue(!!mv);
    assertEquals('5', mv.getAttribute('max-tiles'));

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      omniboxEverywhereMostVisitedEnabled: true,
      smallLoomnibox: false,
      initialFreStage: 0,
    });
    const normalApp = document.createElement('omnibox-everywhere-app');
    document.body.appendChild(normalApp);
    await microtasksFinished();

    assertFalse(normalApp.hasAttribute('small-loomnibox'));
    const normalMv = normalApp.shadowRoot.querySelector('cr-most-visited')!;
    assertTrue(!!normalMv);
    assertEquals('7', normalMv.getAttribute('max-tiles'));
  });

  test(
      'most visited tiles hide-title reflects hideTitle_ property',
      async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({
          omniboxEverywhereMostVisitedEnabled: true,
          omniboxEverywhereShowShortcuts: true,
          omniboxEverywhereMostVisitedHideTitle: true,
          initialFreStage: 0,
        });
        const appWithHiddenTitles =
            document.createElement('omnibox-everywhere-app');
        document.body.appendChild(appWithHiddenTitles);
        await microtasksFinished();

        const mvHidden =
            appWithHiddenTitles.shadowRoot.querySelector('cr-most-visited')!;
        assertTrue(!!mvHidden);
        assertTrue(mvHidden.hasAttribute('hide-title'));

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({
          omniboxEverywhereMostVisitedEnabled: true,
          omniboxEverywhereShowShortcuts: true,
          omniboxEverywhereMostVisitedHideTitle: false,
          initialFreStage: 0,
        });
        const appWithVisibleTitles =
            document.createElement('omnibox-everywhere-app');
        document.body.appendChild(appWithVisibleTitles);
        await microtasksFinished();

        const mvShown =
            appWithVisibleTitles.shadowRoot.querySelector('cr-most-visited')!;
        assertTrue(!!mvShown);
        assertFalse(mvShown.hasAttribute('hide-title'));
      });

  test(
      'close-composebox event exits composebox mode and focuses searchbox',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        assertTrue(!!searchbox);

        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);
        assertFalse(
            !!app.shadowRoot.querySelector('omnibox-everywhere-omnibox'));

        composebox.fire('close-composebox');
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
        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
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
        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox');
        assertTrue(!!composebox);
        assertFalse(app.hasAttribute('show-voice-search-overlay_'));

        composebox.fire('open-voice-search');
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

  test(
      'window focus event focuses searchbox input in searchbox mode',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        assertTrue(!!searchbox);

        let focusCalled = false;
        searchbox.focusInput = () => {
          focusCalled = true;
        };

        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        assertTrue(focusCalled);
      });

  test(
      'window focus event focuses composebox input in composebox mode',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        let focusCalled = false;
        composebox.focusInput = () => {
          focusCalled = true;
        };

        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        assertTrue(focusCalled);
      });

  test(
      'window focus event does not focus input when voice search dialog is ' +
          'open',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire('open-voice-search');
        await microtasksFinished();

        let focusCalled = false;
        searchbox.focusInput = () => {
          focusCalled = true;
        };

        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        assertFalse(focusCalled);
      });

  test(
      'window focus event does not focus input when FRE intro modal is open',
      async () => {
        testProxy.page.setFreState({
          stage: FreStage.kIntroModal,
          currentHotkeyTokens: [],
        });
        await microtasksFinished();

        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        let focusCalled = false;
        searchbox.focusInput = () => {
          focusCalled = true;
        };

        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        assertFalse(focusCalled);
      });

  test(
      'click upon window activation focuses searchbox input in searchbox mode',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        assertTrue(!!searchbox);

        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        let focusCalled = false;
        searchbox.focusInput = () => {
          focusCalled = true;
        };

        blurActiveElement();
        app.click();
        await microtasksFinished();

        assertTrue(focusCalled);
      });

  test(
      'click upon window activation focuses composebox input in ' +
          'composebox mode',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
        await microtasksFinished();

        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);

        let focusCalled = false;
        composebox.focusInput = () => {
          focusCalled = true;
        };

        blurActiveElement();
        app.click();
        await microtasksFinished();

        assertTrue(focusCalled);
      });

  test(
      'click upon activation does not steal focus from a focused control',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;

        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        let focusCalled = false;
        searchbox.focusInput = () => {
          focusCalled = true;
        };

        // Emulate the activating click landing on an interactive control,
        // which takes focus before the click event is dispatched.
        const button = document.createElement('button');
        document.body.appendChild(button);
        button.focus();
        button.click();
        await microtasksFinished();
        button.remove();

        assertFalse(focusCalled);
      });

  test('click on the background outside the app refocuses input', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;

    window.dispatchEvent(new Event('focus'));
    await microtasksFinished();

    let focusCalled = false;
    searchbox.focusInput = () => {
      focusCalled = true;
    };

    // The app element does not fill the window, so an activating click
    // can land on the body padding that accommodates the drop shadow.
    blurActiveElement();
    document.body.click();
    await microtasksFinished();

    assertTrue(focusCalled);
  });

  test('clicking while already active does not refocus input', async () => {
    window.dispatchEvent(new Event('focus'));
    await microtasksFinished();

    // Consume the initial activation click.
    app.click();
    await microtasksFinished();

    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    assertTrue(!!searchbox);

    let focusCalled = false;
    searchbox.focusInput = () => {
      focusCalled = true;
    };

    app.click();
    await microtasksFinished();

    assertFalse(focusCalled);
  });

  test(
      'click upon activation does not focus input when voice search ' +
          'dialog is open',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire('open-voice-search');
        await microtasksFinished();

        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();

        let focusCalled = false;
        searchbox.focusInput = () => {
          focusCalled = true;
        };

        app.click();
        await microtasksFinished();

        assertFalse(focusCalled);
      });

  test(
      'click after the activation window expires does not refocus input',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;

        // Use a mock timer so the activation window can be elapsed without
        // waiting on a real timeout.
        const mockTimer = new MockTimer();
        mockTimer.install();
        window.dispatchEvent(new Event('focus'));
        mockTimer.tick(500);
        mockTimer.uninstall();
        await microtasksFinished();

        let focusCalled = false;
        searchbox.focusInput = () => {
          focusCalled = true;
        };

        app.click();
        await microtasksFinished();

        assertFalse(focusCalled);
      });

  test('click after window blur does not refocus input', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;

    window.dispatchEvent(new Event('focus'));
    await microtasksFinished();

    // Blurring discards the pending activation, so a subsequent click is no
    // longer treated as part of an activation gesture.
    window.dispatchEvent(new Event('blur'));
    await microtasksFinished();

    let focusCalled = false;
    searchbox.focusInput = () => {
      focusCalled = true;
    };

    app.click();
    await microtasksFinished();

    assertFalse(focusCalled);
  });

  test('addFileContext Mojo event updates composebox thumbnail', async () => {
    const omniboxElement =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    const mockToken: UnguessableToken = '1234567890ABCDEF1234567890ABCDEF';

    omniboxElement.fire('open-composebox', {
      text: '',
      mode: 0,
      model: 0,
      smartTabSharingActive: false,
      files: [],
    });
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
      'dismissing shortcut setup chin calls dismissFre; reminder chin ' +
          'renders tokens',
      async () => {
        testProxy.page.setFreState({
          stage: FreStage.kShortcutSetupChin,
          currentHotkeyTokens: ['Alt', 'Space'],
        });
        await microtasksFinished();

        const setupChin = app.shadowRoot?.querySelector<FreChinElement>(
            '#freShortcutSetupChin');
        assertTrue(!!setupChin);
        assertFalse(setupChin.classList.contains('dismissing'));

        const closeBtn =
            setupChin.shadowRoot?.querySelector<HTMLElement>('.close-button');
        assertTrue(!!closeBtn);
        closeBtn.click();
        await microtasksFinished();

        setupChin.dispatchEvent(new AnimationEvent('animationend', {
          animationName: 'fadeOutFre',
        }));
        await microtasksFinished();

        const dismissedStage = await testProxy.handler.whenCalled('dismissFre');
        assertEquals(FreStage.kShortcutSetupChin, dismissedStage);

        testProxy.page.setFreState({
          stage: FreStage.kShortcutReminderChin,
          currentHotkeyTokens: ['Alt', 'Space'],
        });
        await microtasksFinished();

        const reminderChin = app.shadowRoot?.querySelector<FreChinElement>(
            '#freShortcutReminderChin');
        assertTrue(!!reminderChin);
        assertFalse(reminderChin.classList.contains('dismissing'));
        assertEquals(FreChinMode.SHORTCUT_REMINDER, reminderChin.mode);
        assertEquals(2, reminderChin.hotkeyTokens.length);
        assertEquals('Alt', reminderChin.hotkeyTokens[0]);
        assertEquals('Space', reminderChin.hotkeyTokens[1]);
      });

  test(
      'escape key with text in composebox clears text and stays in composebox',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire(
            'open-composebox',
            {text: 'hello world', files: [], mode: 0, model: 0});
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);
        assertEquals('hello world', composebox.input);

        window.dispatchEvent(new KeyboardEvent(
            'keydown', {key: 'Escape', bubbles: true, cancelable: true}));
        await microtasksFinished();

        assertTrue(
            !!app.shadowRoot.querySelector('omnibox-everywhere-composebox'));
        assertEquals('', composebox.input);
        assertEquals(0, testProxy.handler.getCallCount('onEscapePressed'));
      });

  test(
      'escape key with empty composebox exits composebox mode to searchbox',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.fire(
            'open-composebox', {text: '', files: [], mode: 0, model: 0});
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox')!;
        assertTrue(!!composebox);
        assertEquals('', composebox.input);

        window.dispatchEvent(new KeyboardEvent(
            'keydown', {key: 'Escape', bubbles: true, cancelable: true}));
        await microtasksFinished();

        assertFalse(
            !!app.shadowRoot.querySelector('omnibox-everywhere-composebox'));
        const restoredSearchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        assertTrue(!!restoredSearchbox);
        assertEquals(0, testProxy.handler.getCallCount('onEscapePressed'));
      });

  test('escape key with text in searchbox clears searchbox text', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    searchbox.setInputText('searchbox query');
    assertEquals('searchbox query', searchbox.$.input.getInputValue());

    window.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Escape', bubbles: true, cancelable: true}));
    await microtasksFinished();

    assertEquals('', searchbox.$.input.getInputValue());
    assertEquals(0, testProxy.handler.getCallCount('onEscapePressed'));
  });

  test('escape key with empty searchbox calls onEscapePressed', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    assertEquals('', searchbox.$.input.getInputValue());

    window.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Escape', bubbles: true, cancelable: true}));
    await microtasksFinished();

    assertEquals(1, testProxy.handler.getCallCount('onEscapePressed'));
  });

  test(
      'escape key with empty searchbox when FRE modal is showing calls ' +
          'onEscapePressed',
      async () => {
        testProxy.page.setFreState({
          stage: FreStage.kIntroModal,
          currentHotkeyTokens: [],
        });
        await testProxy.page.$.flushForTesting();
        await microtasksFinished();

        assertTrue(!!app.shadowRoot.querySelector('fre-modal'));
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        assertEquals('', searchbox.$.input.getInputValue());

        window.dispatchEvent(new KeyboardEvent(
            'keydown', {key: 'Escape', bubbles: true, cancelable: true}));
        await microtasksFinished();

        assertEquals(1, testProxy.handler.getCallCount('onEscapePressed'));
      });

  test(
      'hotkey dropdown open state prevents blur from removing is-active',
      async () => {
        window.dispatchEvent(new Event('focus'));
        await microtasksFinished();
        assertTrue(app.hasAttribute('is-active'));

        const nativeHasFocus = document.hasFocus;
        document.hasFocus = () => false;

        try {
          testProxy.page.setFreState({
            stage: FreStage.kShortcutSetupChin,
            currentHotkeyTokens: ['Alt', 'Space'],
          });
          await microtasksFinished();

          const setupChin = app.shadowRoot?.querySelector<FreChinElement>(
              '#freShortcutSetupChin');
          assertTrue(!!setupChin);

          const dropdownTrigger =
              setupChin.shadowRoot?.querySelector<HTMLElement>(
                  '.dropdown-trigger');
          assertTrue(!!dropdownTrigger);
          assertEquals('false', dropdownTrigger.getAttribute('aria-expanded'));

          let resolveDropdown: () => void;
          testProxy.handler.showHotkeyDropdown = () =>
              new Promise<void>((resolve) => {
                resolveDropdown = resolve;
              });

          dropdownTrigger.click();
          await microtasksFinished();

          assertEquals('true', dropdownTrigger.getAttribute('aria-expanded'));

          // Blur while dropdown is open does not remove is-active.
          window.dispatchEvent(new Event('blur'));
          await microtasksFinished();
          assertTrue(app.hasAttribute('is-active'));

          // Dropdown closes.
          resolveDropdown!();
          await microtasksFinished();

          // After settling, inactive state applies because document does not
          // have focus.
          await new Promise(resolve => setTimeout(resolve, 0));
          await app.updateComplete;
          assertFalse(app.hasAttribute('is-active'));
          assertEquals('false', dropdownTrigger.getAttribute('aria-expanded'));

          // Focus restores is-active.
          window.dispatchEvent(new Event('focus'));
          await microtasksFinished();
          assertTrue(app.hasAttribute('is-active'));
        } finally {
          document.hasFocus = nativeHasFocus;
        }
      });

  test('inactive state on blur is gated by isPersistentMode', async () => {
    // In persistent mode (default in setup), blur removes is-active.
    window.dispatchEvent(new Event('focus'));
    await microtasksFinished();
    assertTrue(app.hasAttribute('is-active'));

    window.dispatchEvent(new Event('blur'));
    await microtasksFinished();
    assertFalse(app.hasAttribute('is-active'));

    // In ephemeral mode, blur never removes is-active.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({isPersistentMode: false});
    const ephemeralApp = document.createElement('omnibox-everywhere-app');
    document.body.appendChild(ephemeralApp);
    await microtasksFinished();
    assertTrue(ephemeralApp.hasAttribute('is-active'));

    window.dispatchEvent(new Event('blur'));
    await microtasksFinished();
    assertTrue(ephemeralApp.hasAttribute('is-active'));
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
      profileTooltipHeader: 'Chrome profile',
      omniboxEverywhereProfilePickerEnabled: profilePickerEnabled,
      isEnterpriseProfile: false,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    profileIcon = document.createElement('omnibox-everywhere-profile-icon');
    document.body.appendChild(profileIcon);
    await microtasksFinished();
  }

  test(
      'profile icon is not clickable when profile picker is disabled',
      async () => {
        await createProfileIcon(false);
        const container = profileIcon.shadowRoot.querySelector<HTMLElement>(
            '#profileContainer');
        assertTrue(!!container);
        assertEquals('BUTTON', container.tagName);
        assertFalse(container.classList.contains('clickable'));
        assertEquals('true', container.getAttribute('aria-disabled'));
        assertEquals(
            'Chrome profile Test Profile test@example.com',
            container.getAttribute('aria-label'));
        assertEquals(
            'Chrome profile\nTest Profile\ntest@example.com',
            container.getAttribute('title'));
      });

  test('profile icon is clickable when profile picker is enabled', async () => {
    await createProfileIcon(true);
    const container =
        profileIcon.shadowRoot.querySelector<HTMLElement>('#profileContainer');
    assertTrue(!!container);
    assertTrue(container.classList.contains('clickable'));
    assertEquals('false', container.getAttribute('aria-disabled'));
    assertEquals(
        'Chrome profile\nTest Profile\ntest@example.com',
        container.getAttribute('title'));
  });

  test('profile icon image has correct size', async () => {
    await createProfileIcon(false);
    const img =
        profileIcon.shadowRoot.querySelector<HTMLElement>('#profileIcon');
    assertTrue(!!img);
    assertEquals('20px', window.getComputedStyle(img).width);
    assertEquals('20px', window.getComputedStyle(img).height);
  });

  test('renders enterprise badge when profile is enterprise', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: true,
      isEnterpriseProfile: true,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    profileIcon = document.createElement('omnibox-everywhere-profile-icon');
    document.body.appendChild(profileIcon);
    await microtasksFinished();

    assertTrue(profileIcon.hasAttribute('is-enterprise-profile'));
    const enterpriseBadge =
        profileIcon.shadowRoot.querySelector<HTMLElement>('#enterpriseBadge');
    assertTrue(!!enterpriseBadge);
  });
});

suite('OmniboxEverywhereContextMenuTest', () => {
  let app: OmniboxEverywhereAppElement;
  let testProxy: TestSearchboxBrowserProxy;
  let testEverywhereProxy: TestOmniboxEverywhereBrowserProxy;

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
      composeboxEnergyEffectAnimationEnabled: false,
      searchboxCr23Theming: true,
      searchboxCr23SteadyStateShadow: false,
      contextManagementInComposeboxEnabled: false,
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: false,
      searchboxLayoutMode: 'TallBottomContext',
      isPersistentMode: true,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    testEverywhereProxy = new TestOmniboxEverywhereBrowserProxy();
    OmniboxEverywhereBrowserProxyImpl.setInstance(testEverywhereProxy);
    const mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockPageHandler,
        testProxy.handler as unknown as SearchboxPageHandlerRemote,
        testProxy.callbackRouter as unknown as SearchboxPageCallbackRouter));

    app = document.createElement('omnibox-everywhere-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('clicking entrypoint triggers showContextActionMenu', async () => {
    const omnibox = app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    const entrypoint =
        omnibox.shadowRoot.querySelector<ContextualEntrypointButtonElement>(
            '#context')!;
    assertTrue(!!entrypoint);

    entrypoint.fire('context-menu-entrypoint-click', {
      anchorRect: {x: 10, y: 20, width: 30, height: 40},
    });

    const args =
        await testEverywhereProxy.handler.whenCalled('showContextActionMenu');
    assertEquals(10, args.x);
    assertEquals(20, args.y);
    assertEquals(30, args.width);
    assertEquals(40, args.height);
  });

  test(
      'openComposebox with tool Mojo listener switches app to composebox mode',
      async () => {
        assertFalse(app.hasAttribute('is-composebox-mode_'));
        testEverywhereProxy.page.openComposebox({
          tool: ToolMode.kDeepSearch,
          model: ModelMode.kUnspecified,
          tab: null,
          fileToken: null,
          fileInfo: null,
        });
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox');
        assertTrue(!!composebox);
        assertEquals(1, testProxy.handler.getCallCount('setActiveToolMode'));
        assertEquals(
            ToolMode.kDeepSearch,
            testProxy.handler.getArgs('setActiveToolMode')[0][0]);
      });

  test(
      'openComposebox with tab Mojo listener adds tab to composebox',
      async () => {
        assertFalse(app.hasAttribute('is-composebox-mode_'));
        testEverywhereProxy.page.openComposebox({
          tool: ToolMode.kUnspecified,
          model: ModelMode.kUnspecified,
          tab: {
            tabId: 123,
            title: 'Test Tab Title',
            url: 'https://example.com',
            showInCurrentTabChip: false,
            showInPreviousTabChip: false,
            lastActive: {internalValue: 0n},
          },
          fileToken: null,
          fileInfo: null,
        });
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox');
        assertTrue(!!composebox);
      });

  test(
      'openComposebox with file Mojo listener adds file to composebox',
      async () => {
        testEverywhereProxy.page.openComposebox({
          tool: ToolMode.kUnspecified,
          model: ModelMode.kUnspecified,
          tab: null,
          fileToken: '00000000000000010000000000000002',
          fileInfo: {
            fileName: 'test_image.png',
            mimeType: 'image/png',
            imageDataUrl: 'data:image/png;base64,AAAA',
            thumbnailUrl: null,
            isDeletable: true,
            selectionTime: new Date(),
          },
        });
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox');
        assertTrue(!!composebox);
        assertEquals(1, composebox.files.size);
        const attachment = composebox.files.values().next().value;
        assertTrue(!!attachment);
        assertEquals('test_image.png', attachment.name);
        assertEquals('image/png', attachment.type);
      });

  test(
      'file upload validation error displays error scrim in composebox',
      async () => {
        const testToken = '00000000000000010000000000000002';
        testEverywhereProxy.page.openComposebox({
          tool: ToolMode.kUnspecified,
          model: ModelMode.kUnspecified,
          tab: null,
          fileToken: testToken,
          fileInfo: {
            fileName: 'large_image.png',
            mimeType: 'image/png',
            imageDataUrl: 'data:image/png;base64,AAAA',
            thumbnailUrl: null,
            isDeletable: true,
            selectionTime: new Date(),
          },
        });
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox');
        assertTrue(!!composebox);

        testProxy.page.onContextualInputStatusChanged(
            testToken, ContextUploadStatus.kValidationFailed,
            ContextUploadErrorType.kBrowserProcessingFileTooLargeError);
        await microtasksFinished();

        const errorScrim =
            composebox.shadowRoot.querySelector('ntp-error-scrim');
        assertTrue(!!errorScrim);

        errorScrim.fire('dismiss-error-scrim');
        await microtasksFinished();

        assertFalse(!!composebox.shadowRoot.querySelector('ntp-error-scrim'));
      });

  test(
      'max images exceeded error displays error scrim and button dismisses',
      async () => {
        const testToken = '00000000000000010000000000000003';
        testEverywhereProxy.page.openComposebox({
          tool: ToolMode.kUnspecified,
          model: ModelMode.kUnspecified,
          tab: null,
          fileToken: testToken,
          fileInfo: {
            fileName: 'eleventh_image.png',
            mimeType: 'image/png',
            imageDataUrl: null,
            thumbnailUrl: null,
            isDeletable: true,
            selectionTime: new Date(),
          },
        });
        await microtasksFinished();

        const composebox =
            app.shadowRoot.querySelector('omnibox-everywhere-composebox');
        assertTrue(!!composebox);

        testProxy.page.onContextualInputStatusChanged(
            testToken, ContextUploadStatus.kValidationFailed,
            ContextUploadErrorType.kBrowserProcessingMaxImagesExceededError);
        await microtasksFinished();

        const errorScrim =
            composebox.shadowRoot.querySelector('ntp-error-scrim');
        assertTrue(!!errorScrim);

        const dismissBtn = errorScrim.shadowRoot.querySelector<HTMLElement>(
            '#dismissErrorButton');
        assertTrue(!!dismissBtn);
        dismissBtn.click();
        await microtasksFinished();

        assertFalse(!!composebox.shadowRoot.querySelector('ntp-error-scrim'));
        assertEquals(0, composebox.files.size);
      });
});
