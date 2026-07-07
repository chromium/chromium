// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SubmitButtonIconType} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {WindowProxy as CrWindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle} from '../test_support.js';

import {ADD_FILE_CONTEXT_FN, createComposeboxElement, FAKE_TOKEN_STRING, getSubmitContainer, getSubmitIcon, MockInputState, setupComposeboxTest} from './test_support.js';

// ==========================================================
// 1. BASE SUITE (Runs ONLY on cr-composebox element)
// ==========================================================
suite('NewTabPageComposeboxTest', () => {
  const testProxy = setupComposeboxTest();

  setup(() => {
    loadTimeData.overrideValues({
      useNtpComposeboxFork: false,
    });
  });

  test('lens icon click calls handler', async () => {
    createComposeboxElement(testProxy);

    const lensIcon = $$<HTMLElement>(testProxy.element, '#lensIcon');

    lensIcon!.click();
    await testProxy.handler.whenCalled('handleFileUpload');
    assertEquals(1, testProxy.handler.getCallCount('handleFileUpload'));
    const [isImage] = testProxy.handler.getArgs('handleFileUpload');
    assertTrue(isImage);
  });

  test('lens icon mousedown prevents default', async () => {
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const lensIcon = $$<HTMLElement>(testProxy.element, '#lensIcon');

    const event = new MouseEvent(
        'mousedown', {bubbles: true, cancelable: true, composed: true});
    lensIcon!.dispatchEvent(event);
    await microtasksFinished();

    assertTrue(event.defaultPrevented);
  });

  test(
      'cr-composebox-submit is rendered when searchboxNextEnabled is false',
      async () => {
        createComposeboxElement(testProxy, {
          searchboxNextEnabled: false,
        });
        await microtasksFinished();

        const composeboxSubmit =
            testProxy.element.shadowRoot.querySelector('cr-composebox-submit');

        assertTrue(!!composeboxSubmit);
      });

  test(
      'cr-composebox-submit is rendered when searchboxLayoutMode is TallBottomContext',
      async () => {
        createComposeboxElement(testProxy, {
          searchboxNextEnabled: true,
        });
        testProxy.element.searchboxLayoutMode = 'TallBottomContext';
        testProxy.element.getInputElement().$.input.value = 'test';
        testProxy.element.getInputElement().$.input.dispatchEvent(
            new Event('input'));
        await microtasksFinished();

        const composeboxSubmit =
            testProxy.element.shadowRoot.querySelector('cr-composebox-submit');

        assertTrue(!!composeboxSubmit);
      });

  test('empty input binds disabled attribute on submit button', async () => {
    createComposeboxElement(testProxy);

    testProxy.element.getInputElement().$.input.value = '';
    testProxy.element.getInputElement().$.input.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    const submitButton = getSubmitIcon(testProxy);
    assertTrue(submitButton.hasAttribute('disabled'));
  });

  test('isCollapsible attribute sets expanding state when true', async () => {
    createComposeboxElement(testProxy);
    const collapsibleBox = testProxy.element;
    collapsibleBox.isCollapsible = true;
    document.body.appendChild(collapsibleBox);
    await collapsibleBox.updateComplete;

    const collapsibleInput = collapsibleBox.getInputElement().$.input;
    collapsibleBox.$.composebox.dispatchEvent(new FocusEvent('focusin'));
    await collapsibleBox.updateComplete;
    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should be expanded initially due to focus event');

    collapsibleBox.$.composebox.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await collapsibleBox.updateComplete;
    assertFalse(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should collapse on blur without text');

    collapsibleBox.$.composebox.dispatchEvent(new FocusEvent('focusin'));
    await collapsibleBox.updateComplete;
    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should expand on focus');

    // Set text and re-test blur logic
    collapsibleInput.value = 'some text';
    collapsibleInput.dispatchEvent(new Event('input'));
    await collapsibleBox.updateComplete;

    collapsibleBox.$.composebox.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await collapsibleBox.updateComplete;
    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should stay expanded on blur with text');
  });

  test('isCollapsible attribute sets expanded state with file', async () => {
    createComposeboxElement(testProxy);
    testProxy.element.isCollapsible = true;
    await microtasksFinished();

    testProxy.element.$.composebox.dispatchEvent(new FocusEvent('focusin'));
    await testProxy.element.updateComplete;
    assertTrue(
        testProxy.element.hasAttribute('expanding_'),
        'Collapsible should be expanded initially due to focus event');

    // Initially, carousel is not shown.
    assertFalse(testProxy.element.hasAttribute('show-file-carousel'));

    // Set a thumbnail.
    const thumbnailUrl = 'data:image/png;base64,sometestdata';
    testProxy.searchboxCallbackRouterRemote.addFileContext(FAKE_TOKEN_STRING, {
      fileName: 'Visual Selection',
      mimeType: 'image/png',
      imageDataUrl: thumbnailUrl,
      isDeletable: true,
      selectionTime: new Date(),
    } as SelectedFileInfo);
    await microtasksFinished();

    // Assert thumbnail is shown.
    assertTrue(testProxy.element.hasAttribute('show-file-carousel'));
    const fileCarousel = testProxy.element.$.carousel;
    await microtasksFinished();

    testProxy.element.$.composebox.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await testProxy.element.updateComplete;
    assertTrue(
        testProxy.element.hasAttribute('expanding_'),
        'Collapsible should remain expanded on blur with file');

    // Delete the thumbnail.
    const fileThumbnail =
        fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
    assertTrue(!!fileThumbnail);

    const removeImgButton =
        fileThumbnail.shadowRoot.querySelector<HTMLElement>('#removeImgButton');
    removeImgButton!.click();
    await microtasksFinished();

    // Focus the composebox again.
    testProxy.element.$.composebox.dispatchEvent(new FocusEvent('focusin'));
    await testProxy.element.updateComplete;
    assertTrue(
        testProxy.element.hasAttribute('expanding_'),
        'Collapsible should still expand when focused in');

    // Blur the composebox again.
    testProxy.element.$.composebox.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await testProxy.element.updateComplete;
    assertFalse(
        testProxy.element.hasAttribute('expanding_'),
        'Collapsible should collapse on blur with no file');
  });

  test('isCollapsible attribute sets expanded state when false', async () => {
    createComposeboxElement(testProxy);
    const collapsibleBox = testProxy.element;
    const collapsibleInput = collapsibleBox.getInputElement().$.input;
    collapsibleBox.isCollapsible = false;
    await collapsibleBox.updateComplete;

    // Blur the input first, since connectedCallback focuses it by
    // default. This ensures the component is in a state where it can be
    // collapsed.
    collapsibleInput.blur();
    await collapsibleBox.updateComplete;

    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Non-collapsible should be expanded');
  });

  test('collapsible composebox collapses after query submitted', async () => {
    createComposeboxElement(testProxy);
    const collapsibleBox = testProxy.element;
    const collapsibleInput = collapsibleBox.getInputElement().$.input;
    collapsibleBox.isCollapsible = true;
    await collapsibleBox.updateComplete;

    collapsibleInput.focus();
    collapsibleInput.value = 'some text';
    collapsibleInput.dispatchEvent(new Event('input'));
    await collapsibleBox.updateComplete;
    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should be expanded before submit');

    // Mock an autocomplete result to allow submission.
    const matches =
        [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'some text',
          matches,
        }));
    await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
    await collapsibleBox.updateComplete;

    // Submit query.
    const composeboxSubmit =
        collapsibleBox.shadowRoot.querySelector('cr-composebox-submit');
    assertTrue(!!composeboxSubmit);
    const submit = composeboxSubmit.shadowRoot.querySelector<HTMLElement>(
        '#submitContainer');
    assertTrue(!!submit);
    submit.click();
    await collapsibleBox.updateComplete;
    await microtasksFinished();

    // Submit container should be disabled.
    assertStyle(getSubmitContainer(testProxy), 'cursor', 'not-allowed');
    assertEquals('', collapsibleInput.value, 'Input should be cleared');
  });

  test(
      'submit disabled when tool is Deep Search (default entrypoint)',
      async () => {
        createComposeboxElement(testProxy);

        assertEquals(
            testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'),
            0);

        // Default: submit is disabled with empty input, clicking does
        // nothing.
        getSubmitContainer(testProxy).click();
        await microtasksFinished();
        assertEquals(
            testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'),
            0);

        // Change tool to Deep Search
        const inputState = new MockInputState({
          activeTool: ToolMode.kDeepSearch,
        });
        testProxy.searchboxCallbackRouterRemote.onInputStateChanged(inputState);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

        await microtasksFinished();

        // Submit should still be DISABLED because entrypoint is not
        // ContextualTasks.
        getSubmitContainer(testProxy).click();
        await microtasksFinished();
        assertEquals(testProxy.searchboxHandler.getCallCount('submitQuery'), 0);
      });

  test('clear functionality', async () => {
    createComposeboxElement(testProxy);
    testProxy.searchboxHandler.setPromiseResolveFor(
        ADD_FILE_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

    // Check submit button disabled.
    assertStyle(getSubmitContainer(testProxy), 'cursor', 'not-allowed');
    // Add input.
    testProxy.element.getInputElement().$.input.value = 'test';
    testProxy.element.getInputElement().$.input.dispatchEvent(
        new Event('input'));
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo1'], 'foo1.pdf', {type: 'application/pdf'}));
    testProxy.element.$.fileInputs.$.fileInput.files = dataTransfer.files;
    testProxy.element.$.fileInputs.$.fileInput.dispatchEvent(
        new Event('change'));

    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    /* Submit button will not be enabled since frontend has not been
     * notified that file is done uploading. Carousel should
     * still have the file marked as added.
     */
    assertEquals(testProxy.element.$.carousel.files.length, 1);

    // Clear input.
    $$<HTMLElement>(
        testProxy.element.getInputElement(), '#cancelIcon')!.click();
    await microtasksFinished();

    // Assert
    assertEquals(testProxy.searchboxHandler.getCallCount('clearFiles'), 1);

    // Check submit button disabled and files empty.
    assertStyle(getSubmitContainer(testProxy), 'cursor', 'not-allowed');
    assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

    // Close composebox.
    testProxy.element.suggestInventory = SuggestInventory.kTravel;
    assertEquals(SuggestInventory.kTravel, testProxy.element.suggestInventory);

    const whenCloseComposebox =
        eventToPromise<CustomEvent<{composeboxText: string}>>(
            'close-composebox', testProxy.element);
    $$<HTMLElement>(
        testProxy.element.getInputElement(), '#cancelIcon')!.click();
    await whenCloseComposebox;
    assertEquals(testProxy.searchboxHandler.getCallCount('clearFiles'), 2);
    assertEquals(null, testProxy.element.suggestInventory);
  });
});

// =========================================================================
// 2. COMMON SUITE (Runs on both ntp-composebox and cr-composebox elements)
// =========================================================================
[true, false].forEach(useForked => {
  suite(`NewTabPageComposeboxTestV2 (useNtpComposeboxFork = ${useForked})`, () => {
    const testProxy = setupComposeboxTest();

    setup(() => {
      loadTimeData.overrideValues({
        useNtpComposeboxFork: useForked,
      });
    });

    test(
        'sets darkThemeColorsEnabled as false on search-animated-glow',
        async () => {
          createComposeboxElement(testProxy);
          await microtasksFinished();

          const animatedGlow = testProxy.element.shadowRoot
                                   .querySelector<SearchAnimatedGlowElement>(
                                       'search-animated-glow');
          assertTrue(!!animatedGlow);
          assertFalse(animatedGlow.darkThemeColorsEnabled);
        });

    test('ntp composebox uses configured forward submit icon', async () => {
      createComposeboxElement(testProxy, {
        searchboxNextEnabled: true,
        submitButtonIconType: SubmitButtonIconType.FORWARD,
      });
      testProxy.element.searchboxLayoutMode = 'Compact';
      await microtasksFinished();

      testProxy.element.getInputElement().$.input.value = 'test';
      testProxy.element.getInputElement().$.input.dispatchEvent(
          new Event('input'));
      await microtasksFinished();

      const submitIcon = getSubmitIcon(testProxy);
      assertTrue(submitIcon.classList.contains('icon-arrow-forward'));
      assertFalse(submitIcon.classList.contains('icon-arrow-upward'));
    });

    test('composebox defaults to forward submit icon', async () => {
      createComposeboxElement(testProxy, {
        searchboxNextEnabled: true,
      });
      testProxy.element.searchboxLayoutMode = 'Compact';
      await microtasksFinished();

      testProxy.element.getInputElement().$.input.value = 'test';
      testProxy.element.getInputElement().$.input.dispatchEvent(
          new Event('input'));
      await microtasksFinished();

      const submitIcon = getSubmitIcon(testProxy);
      assertTrue(submitIcon.classList.contains('icon-arrow-upward'));
    });

    if (useForked) {
      test(
          'submit disabled when tool is Deep Search (default entrypoint) - ntp-composebox only',
          async () => {
            createComposeboxElement(testProxy, {
              searchboxNextEnabled: true,
            });
            await microtasksFinished();

            // In modern layout, empty input submit button is omitted from DOM.
            assertFalse(!!testProxy.element.shadowRoot.querySelector(
                'cr-composebox-submit'));

            // Change tool to Deep Search
            const inputState = new MockInputState({
              activeTool: ToolMode.kDeepSearch,
            });
            testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
                inputState);
            await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
            await microtasksFinished();

            // Assert button is still not in DOM.
            assertFalse(!!testProxy.element.shadowRoot.querySelector(
                'cr-composebox-submit'));
          });

      test('clear functionality - ntp-composebox only', async () => {
        createComposeboxElement(testProxy, {
          searchboxNextEnabled: true,
        });
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_FILE_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});
        await microtasksFinished();

        // Assert submit button is omitted from DOM.
        assertFalse(!!testProxy.element.shadowRoot.querySelector(
            'cr-composebox-submit'));

        // Add input and files.
        testProxy.element.getInputElement().$.input.value = 'test';
        testProxy.element.getInputElement().$.input.dispatchEvent(
            new Event('input'));
        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(
            new File(['foo1'], 'foo1.pdf', {type: 'application/pdf'}));
        testProxy.element.$.fileInputs.$.fileInput.files = dataTransfer.files;
        testProxy.element.$.fileInputs.$.fileInput.dispatchEvent(
            new Event('change'));

        await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
        await microtasksFinished();

        assertEquals(testProxy.element.$.carousel.files.length, 1);

        // Clear input.
        $$<HTMLElement>(
            testProxy.element.getInputElement(), '#cancelIcon')!.click();
        await microtasksFinished();
        assertEquals(testProxy.searchboxHandler.getCallCount('clearFiles'), 1);

        // Assert button is omitted from DOM after clear.
        assertFalse(!!testProxy.element.shadowRoot.querySelector(
            'cr-composebox-submit'));
        assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));
      });

      test('suggestion activity link triggers navigation', async () => {
        createComposeboxElement(testProxy);
        await microtasksFinished();

        const matches = [
          createSearchMatchForTesting({
            isNoncannedAimSuggestion: true,
          }),
        ];
        testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              matches,
            }));
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        await testProxy.element.updateComplete;

        const suggestionActivity =
            testProxy.element.shadowRoot.querySelector('#suggestionActivity');
        assertTrue(!!suggestionActivity);
        const localizedLink =
            suggestionActivity.querySelector('localized-link');
        assertTrue(!!localizedLink);

        const testUrl = 'about:blank?activity';
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

        const url = await testProxy.handler.whenCalled('navigateUrl');
        assertEquals(testUrl, url);
        assertTrue(preventDefaultCalled);
      });

      test(
          'suggestion activity link hidden when suggestions are non canned',
          async () => {
            createComposeboxElement(testProxy);
            await microtasksFinished();

            const matches = [
              createSearchMatchForTesting({
                isNoncannedAimSuggestion: false,
              }),
            ];
            testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
                createAutocompleteResultForTesting({
                  matches,
                }));
            await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
            await microtasksFinished();
            await testProxy.element.updateComplete;

            const suggestionActivity =
                testProxy.element.shadowRoot.querySelector(
                    '#suggestionActivity');
            assertFalse(!!suggestionActivity);
          });
    }

    test(
        'cr-composebox-submit is not rendered when searchboxNextEnabled is true',
        async () => {
          createComposeboxElement(testProxy, {
            searchboxNextEnabled: true,
          });
          await microtasksFinished();

          const composeboxSubmit = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-submit');

          assertFalse(!!composeboxSubmit);
        });

    test(
        'cr-composebox-submit is rendered when searchboxLayoutMode is Compact',
        async () => {
          createComposeboxElement(testProxy, {
            searchboxNextEnabled: true,
          });
          testProxy.element.searchboxLayoutMode = 'Compact';
          testProxy.element.getInputElement().$.input.value = 'test';
          testProxy.element.getInputElement().$.input.dispatchEvent(
              new Event('input'));
          await microtasksFinished();

          const composeboxSubmit = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-submit');

          assertTrue(!!composeboxSubmit);
        });

    test(
        'cr-composebox-submit is not rendered when there is no input text',
        async () => {
          createComposeboxElement(testProxy, {
            searchboxNextEnabled: true,
          });
          testProxy.element.searchboxLayoutMode = 'Compact';
          testProxy.element.getInputElement().$.input.value = '';
          testProxy.element.getInputElement().$.input.dispatchEvent(
              new Event('input'));
          await microtasksFinished();

          const composeboxSubmit = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-submit');

          assertFalse(!!composeboxSubmit);
        });

    test('submit button click leads to handler called', async () => {
      createComposeboxElement(testProxy, {
        searchboxNextEnabled: true,
      });
      testProxy.element.searchboxLayoutMode = 'Compact';
      await microtasksFinished();
      // Assert.
      assertEquals(
          testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

      // Arrange.
      testProxy.element.getInputElement().$.input.value = 'test';
      testProxy.element.getInputElement().$.input.dispatchEvent(
          new Event('input'));
      const matches =
          [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            input: 'test',
            matches,
          }));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      getSubmitContainer(testProxy).click();
      await microtasksFinished();

      // Assert call occurs.
      assertEquals(
          testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 1);
    });

    test('keydown submit only works for enter', async () => {
      createComposeboxElement(testProxy, {
        searchboxNextEnabled: true,
      });
      testProxy.element.searchboxLayoutMode = 'Compact';
      await microtasksFinished();
      // Assert.
      assertEquals(
          testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

      // Arrange.
      testProxy.element.getInputElement().$.input.value = 'test';
      testProxy.element.getInputElement().$.input.dispatchEvent(
          new Event('input'));
      const matches =
          [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            input: 'test',
            matches: matches,
          }));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      const shiftEnterEvent = new KeyboardEvent('keydown', {
        key: 'Enter',
        shiftKey: true,
        bubbles: true,
        cancelable: true,
        composed: true,
      });
      testProxy.element.getInputElement().$.input.dispatchEvent(
          shiftEnterEvent);
      await microtasksFinished();

      // Assert.
      assertEquals(
          testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

      const enterEvent = new KeyboardEvent('keydown', {
        key: 'Enter',
        bubbles: true,
        cancelable: true,
        composed: true,
      });
      testProxy.element.getInputElement().$.input.dispatchEvent(enterEvent);
      await microtasksFinished();

      // Assert call occurs.
      assertEquals(
          testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 1);
    });

    test('ShareComposeboxMountPreservesAutoReposition', async () => {
      createComposeboxElement(testProxy);
      await testProxy.element.updateComplete;

      const entrypointAndMenu =
          testProxy.element.shadowRoot
              .querySelector<ContextualEntrypointAndMenuElement>(
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

    // Required to test how the voice chips are integrated into NTP html
    // (event listeners, id's, classes, etc.):
    suite('voice search', () => {
      setup(async () => {
        const crWindowProxy = TestMock.fromClass(CrWindowProxy);
        crWindowProxy.setResultFor('hasWebkitSpeechRecognition', true);
        crWindowProxy.setResultMapperFor('createSpeechRecognition', () => {
          const mock = new EventTarget() as unknown as
              ReturnType<typeof CrWindowProxy.prototype.createSpeechRecognition>;
          mock.abort = () => {};
          mock.start = () => {};
          mock.stop = () => {};
          return mock;
        });
        crWindowProxy.setResultMapperFor(
            'matchMedia', (query: string) => window.matchMedia(query));
        CrWindowProxy.setInstance(crWindowProxy);

        testProxy.searchboxHandler.setPromiseResolveFor('getPageClassification', {
          metricSource: 'NTP_COMPOSEBOX',
        });

        createComposeboxElement(testProxy);
        testProxy.element.showVoiceSearch = true;
        await testProxy.element.updateComplete;
      });

      async function enterVoiceSearchMode() {
        const voiceSearchButton =
            testProxy.element.shadowRoot.querySelector<HTMLElement>(
                '#voiceSearchButton');
        assertTrue(!!voiceSearchButton);
        voiceSearchButton.click();
        await microtasksFinished();
        await testProxy.element.updateComplete;
      }

      async function submitVoiceSearch() {
        const voiceSearch = testProxy.element.shadowRoot.querySelector(
            'cr-composebox-voice-search');
        assertTrue(!!voiceSearch);

        const mockVoiceSearch = voiceSearch as unknown as {
          finalResult_: string,
          transcript_: string,
        };
        mockVoiceSearch.finalResult_ = 'test query';
        mockVoiceSearch.transcript_ = 'test query';
        voiceSearch.requestUpdate();
        await voiceSearch.updateComplete;

        const submitButton =
            voiceSearch.shadowRoot.querySelector('cr-composebox-submit');
        assertTrue(!!submitButton);
        await submitButton.updateComplete;

        const submitContainer =
            submitButton.shadowRoot.querySelector<HTMLElement>('#submitContainer');
        assertTrue(!!submitContainer);
        submitContainer.click();

        await microtasksFinished();
        await testProxy.element.updateComplete;
        await testProxy.searchboxHandler.whenCalled('submitQuery');
      }

      test(
          'voice error scrim is absolute when not hidden; display none otherwise',
          async () => {
            // When no error: errorScrim should be absent:
            let errorScrim =
                testProxy.element.shadowRoot.querySelector('#errorScrim');
            assertFalse(!!errorScrim);

            // When error: errorScrim is shown, must be position absolute:
            testProxy.element.inVoiceSearchMode = true;
            testProxy.element.errorMessage = 'Network error';
            await testProxy.element.updateComplete;

            errorScrim =
                testProxy.element.shadowRoot.querySelector('#errorScrim');
            assertTrue(!!errorScrim);
            assertEquals(
                'absolute', window.getComputedStyle(errorScrim).position);

            // When dismissed (hidden again):
            const shadowRoot = errorScrim.shadowRoot;
            assertTrue(!!shadowRoot);
            if (!shadowRoot) {
              return;
            }
            const dismissErrorButton =
                shadowRoot.querySelector<HTMLElement>('#dismissErrorButton');
            assertTrue(!!dismissErrorButton);
            dismissErrorButton.click();
            await microtasksFinished();
            await testProxy.element.updateComplete;

            errorScrim =
                testProxy.element.shadowRoot.querySelector('#errorScrim');
            // Equivalent to checking 'display none':
            assertFalse(!!errorScrim);
          });

      test('toolchip and image added, then removed in voice search', async () => {
        // Add tool chip:
        testProxy.element.contextMenuEnabled = true;
        testProxy.element.inToolMode = true;
        testProxy.element.voiceSearchCoherenceEnabled = true;

        // Add image:
        const thumbnailUrl = 'data:image/png;base64,sometestdata';
        const testToken = '12345678901234567890123456789012';
        testProxy.searchboxCallbackRouterRemote.addFileContext(testToken, {
          fileName: 'test.png',
          mimeType: 'image/png',
          imageDataUrl: thumbnailUrl,
          isDeletable: true,
          selectionTime: new Date(),
        } as SelectedFileInfo);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        await testProxy.element.updateComplete;

        // Enter voice search mode:
        await enterVoiceSearchMode();

        // Ensure carousel and toolchip are visible in voice search:
        const animatedGlow =
            testProxy.element.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!animatedGlow);
        const voiceCarouselContainer =
            animatedGlow.querySelector('#voiceCarouselContainer');
        assertTrue(!!voiceCarouselContainer);
        const voiceCarousel =
            voiceCarouselContainer.querySelector('#voiceSearchCarousel');
        assertTrue(!!voiceCarousel);
        const voiceToolChip =
            animatedGlow.querySelector('#voiceToolChipsContainer');
        assertTrue(!!voiceToolChip);

        // Verify CSS order
        assertFalse(voiceCarousel.classList.contains('top'));
        assertEquals('2', window.getComputedStyle(voiceCarouselContainer).order);
        assertEquals('3', window.getComputedStyle(voiceToolChip).order);
        const recordingWave =
            animatedGlow.shadowRoot.querySelector('#recordingWave');
        assertTrue(!!recordingWave);
        assertEquals('1', window.getComputedStyle(recordingWave).order);

        // Remove image:
        const shadowRoot = voiceCarousel.shadowRoot;
        assertTrue(!!shadowRoot);
        if (!shadowRoot) {
          return;
        }
        const fileThumbnail = shadowRoot.querySelector(
            'cr-composebox-file-thumbnail');
        assertTrue(!!fileThumbnail);
        const removeImgButton =
            fileThumbnail.shadowRoot.querySelector<HTMLElement>(
                '#removeImgButton');
        removeImgButton!.click();
        await microtasksFinished();
        await testProxy.element.updateComplete;
        assertEquals(0, testProxy.element.files.size);

        // Remove toolchip:
        testProxy.element.inToolMode = false;
        await testProxy.element.updateComplete;
        assertFalse(!!animatedGlow.querySelector('#voiceToolChipsContainer'));
      });

      test('remove image but submit toolchip in voice search mode', async () => {
        // Add tool chip and image
        testProxy.element.contextMenuEnabled = true;
        testProxy.element.inToolMode = true;
        testProxy.element.voiceSearchCoherenceEnabled = true;
        const thumbnailUrl = 'data:image/png;base64,sometestdata';
        const testToken = '12345678901234567890123456789012';
        testProxy.searchboxCallbackRouterRemote.addFileContext(testToken, {
          fileName: 'test.png',
          mimeType: 'image/png',
          imageDataUrl: thumbnailUrl,
          isDeletable: true,
          selectionTime: new Date(),
        } as SelectedFileInfo);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        await testProxy.element.updateComplete;

        await enterVoiceSearchMode();

        const animatedGlow =
            testProxy.element.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!animatedGlow);
        const voiceCarouselContainer =
            animatedGlow.querySelector('#voiceCarouselContainer');
        assertTrue(!!voiceCarouselContainer);
        const voiceCarousel =
            voiceCarouselContainer.querySelector('#voiceSearchCarousel');
        assertTrue(!!voiceCarousel);

        // Remove image from voice carousel:
        const shadowRoot = voiceCarousel.shadowRoot;
        assertTrue(!!shadowRoot);
        if (!shadowRoot) {
          return;
        }
        const fileThumbnail = shadowRoot.querySelector(
            'cr-composebox-file-thumbnail');
        assertTrue(!!fileThumbnail);
        const removeImgButton =
            fileThumbnail.shadowRoot.querySelector<HTMLElement>(
                '#removeImgButton');
        removeImgButton!.click();
        await microtasksFinished();
        await testProxy.element.updateComplete;
        assertEquals(0, testProxy.element.files.size);

        // Submit:
        await submitVoiceSearch();

        assertTrue(testProxy.element.inToolMode);
        assertEquals(0, testProxy.element.files.size);
      });

      test('remove toolchip but submit image in voice search mode', async () => {
        // Add tool chip and image:
        testProxy.element.contextMenuEnabled = true;
        testProxy.element.inToolMode = true;
        testProxy.element.voiceSearchCoherenceEnabled = true;
        const thumbnailUrl = 'data:image/png;base64,sometestdata';
        const testToken = '12345678901234567890123456789012';
        testProxy.searchboxCallbackRouterRemote.addFileContext(testToken, {
          fileName: 'test.png',
          mimeType: 'image/png',
          imageDataUrl: thumbnailUrl,
          isDeletable: true,
          selectionTime: new Date(),
        } as SelectedFileInfo);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        await testProxy.element.updateComplete;

        await enterVoiceSearchMode();

        const animatedGlow =
            testProxy.element.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!animatedGlow);
        const voiceToolChip =
            animatedGlow.querySelector('#voiceToolChipsContainer');
        assertTrue(!!voiceToolChip);

        // Remove tool chip from voice tool chips container:
        const toolChip = voiceToolChip.querySelector('cr-composebox-tool-chip');
        assertTrue(!!toolChip);
        const toolEnabledButton =
            toolChip.shadowRoot.querySelector<HTMLElement>('#toolEnabledButton');
        assertTrue(!!toolEnabledButton);
        toolEnabledButton.click();
        // Prevent the image file from being cleared on component
        // updates (follows `inputState`):
        testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
            new MockInputState({
              activeTool: ToolMode.kUnspecified,
              allowedInputTypes: [InputType.kLensImage],
            }));
        await microtasksFinished();
        await testProxy.element.updateComplete;
        assertFalse(testProxy.element.inToolMode);

        // Submit:
        await submitVoiceSearch();

        assertFalse(testProxy.element.inToolMode);
        assertEquals(1, testProxy.element.files.size);
      });

      test(
          'removing chips in voice carousel removes them from main carousel after' +
              ' stopping recording',
          async () => {
            // Add tool chip and image
            testProxy.element.contextMenuEnabled = true;
            testProxy.element.inToolMode = true;
            testProxy.element.voiceSearchCoherenceEnabled = true;
            const thumbnailUrl = 'data:image/png;base64,sometestdata';
            const testToken = '12345678901234567890123456789012';
            testProxy.searchboxCallbackRouterRemote.addFileContext(testToken, {
              fileName: 'test.png',
              mimeType: 'image/png',
              imageDataUrl: thumbnailUrl,
              isDeletable: true,
              selectionTime: new Date(),
            } as SelectedFileInfo);
            await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
            await microtasksFinished();
            await testProxy.element.updateComplete;

            // Enter voice search mode by clicking voice search button:
            await enterVoiceSearchMode();

            const animatedGlow = testProxy.element.shadowRoot.querySelector(
                'search-animated-glow');
            assertTrue(!!animatedGlow);
            const voiceCarouselContainer =
                animatedGlow.querySelector('#voiceCarouselContainer');
            assertTrue(!!voiceCarouselContainer);
            const voiceCarousel =
                voiceCarouselContainer.querySelector('#voiceSearchCarousel');
            assertTrue(!!voiceCarousel);
            const voiceToolChip =
                animatedGlow.querySelector('#voiceToolChipsContainer');
            assertTrue(!!voiceToolChip);

            // Remove image from voice carousel:
            const shadowRoot = voiceCarousel.shadowRoot;
            assertTrue(!!shadowRoot);
            if (!shadowRoot) {
              return;
            }
            const fileThumbnail = shadowRoot.querySelector(
                'cr-composebox-file-thumbnail');
            assertTrue(!!fileThumbnail);
            const removeImgButton =
                fileThumbnail.shadowRoot.querySelector<HTMLElement>(
                    '#removeImgButton');
            removeImgButton!.click();
            await microtasksFinished();
            await testProxy.element.updateComplete;
            assertEquals(0, testProxy.element.files.size);

            // Remove tool chip from voice tool chips container:
            const toolChip =
                voiceToolChip.querySelector('cr-composebox-tool-chip');
            assertTrue(!!toolChip);
            const toolEnabledButton =
                toolChip.shadowRoot.querySelector<HTMLElement>(
                    '#toolEnabledButton');
            assertTrue(!!toolEnabledButton);
            toolEnabledButton.click();
            testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
                new MockInputState({
                  activeTool: ToolMode.kUnspecified,
                  allowedInputTypes: [InputType.kLensImage],
                }));
            await microtasksFinished();
            await testProxy.element.updateComplete;
            assertFalse(testProxy.element.inToolMode);

            // Stop recording:
            const voiceSearch = testProxy.element.shadowRoot.querySelector(
                'cr-composebox-voice-search');
            assertTrue(!!voiceSearch);
            const stopButton =
                voiceSearch.shadowRoot.querySelector<HTMLElement>('#stopButton');
            assertTrue(!!stopButton);
            stopButton.click();
            await microtasksFinished();
            await testProxy.element.updateComplete;

            assertFalse(testProxy.element.inToolMode);
            assertEquals(0, testProxy.element.files.size);
          });

      test(
          'voice search and its container are absolute when not waiting and not in error',
          async () => {
            testProxy.element.showVoiceSearch = true;
            await testProxy.element.updateComplete;

            const voiceSearch = testProxy.element.shadowRoot.querySelector(
                'cr-composebox-voice-search');
            assertTrue(!!voiceSearch);

            // Not waiting and not in error:
            testProxy.element.inVoiceSearchMode = true;
            testProxy.element.isListening = true;
            await testProxy.element.updateComplete;
            voiceSearch.isPermissionPromptOpen = false;
            await voiceSearch.updateComplete;

            const voiceSearchContainer =
                voiceSearch.shadowRoot.querySelector('#container');
            assertTrue(!!voiceSearchContainer);

            assertEquals(
                'absolute', window.getComputedStyle(voiceSearch).position);
            assertEquals(
                'absolute',
                window.getComputedStyle(voiceSearchContainer).position);

            // Waiting (permission prompt open):
            voiceSearch.isPermissionPromptOpen = true;
            await voiceSearch.updateComplete;
            assertNotEquals(
                'absolute',
                window.getComputedStyle(voiceSearchContainer).position);

            // In error:
            voiceSearch.isPermissionPromptOpen = false;
            (voiceSearch as unknown as {errorMessage_: string}).errorMessage_ = 'Voice error';
            await voiceSearch.updateComplete;
            assertNotEquals(
                'absolute',
                window.getComputedStyle(voiceSearchContainer).position);
          });
    });
  });
});

// ==========================================================
// 3. RESIZE OBSERVER SUITE
// ==========================================================
suite('NewTabPageComposeboxResizeObserverTest', () => {
  const testProxy = setupComposeboxTest();
  // Keep this aligned with DEBOUNCE_TIMEOUT_MS in composebox.ts.
  const RESIZE_DEBOUNCE_TIMEOUT_MS = 20;
  let originalResizeObserver: typeof ResizeObserver;
  let mockTimer: MockTimer;

  class MockResizeObserver implements ResizeObserver {
    static instances: MockResizeObserver[] = [];
    observedTargets: Element[] = [];
    disconnected = false;

    constructor(private callback: ResizeObserverCallback) {
      MockResizeObserver.instances.push(this);
    }

    disconnect() {
      this.disconnected = true;
    }

    observe(target: Element, _options?: ResizeObserverOptions) {
      this.observedTargets.push(target);
    }

    takeRecords(): ResizeObserverEntry[] {
      return [];
    }

    unobserve(_target: Element) {}

    trigger() {
      this.callback([], this);
    }
  }

  function getObserversForTarget(target: Element): MockResizeObserver[] {
    return MockResizeObserver.instances.filter(
        observer => observer.observedTargets.includes(target));
  }

  function getActiveObserversForTarget(target: Element): MockResizeObserver[] {
    return getObserversForTarget(target).filter(
        observer => !observer.disconnected);
  }

  async function flushComposebox() {
    await testProxy.element.updateComplete;
    await testProxy.element.getInputElement().updateComplete;
    await microtasksFinished();
  }

  setup(() => {
    loadTimeData.overrideValues({
      useNtpComposeboxFork: false,
    });
    originalResizeObserver = window.ResizeObserver;
    window.ResizeObserver =
        MockResizeObserver as unknown as typeof ResizeObserver;
    MockResizeObserver.instances = [];
    mockTimer = new MockTimer();
  });

  teardown(() => {
    window.ResizeObserver = originalResizeObserver;
    mockTimer.uninstall();
  });

  test(
      'observeResize emits composebox resize events for host and dropdown',
      async () => {
        createComposeboxElement(testProxy, {observeResize: true});
        await flushComposebox();

        const hostObserver = getActiveObserversForTarget(testProxy.element);
        const dropdownObserver =
            getActiveObserversForTarget(testProxy.element.$.matches);
        assertEquals(1, hostObserver.length);
        assertEquals(1, dropdownObserver.length);

        const hostResizeEvent = eventToPromise<CustomEvent<{height: number}>>(
            'composebox-resize', testProxy.element);
        hostObserver[0]!.trigger();
        // Advance the debounce used by setupResizeObservers_().
        mockTimer.tick(RESIZE_DEBOUNCE_TIMEOUT_MS);
        await microtasksFinished();
        const hostEvent = await hostResizeEvent;
        assertTrue(hostEvent.detail.height !== undefined);

        const dropdownResizeEvent =
            eventToPromise<CustomEvent<{dropdownHeight: number}>>(
                'composebox-resize', testProxy.element);
        dropdownObserver[0]!.trigger();
        mockTimer.tick(RESIZE_DEBOUNCE_TIMEOUT_MS);
        await microtasksFinished();
        const dropdownEvent = await dropdownResizeEvent;
        assertTrue(dropdownEvent.detail.dropdownHeight !== undefined);
      });

  test('observeResize false skips public resize observers', async () => {
    createComposeboxElement(testProxy, {observeResize: false});
    await flushComposebox();

    const inputWrapper =
        testProxy.element.getInputElement()
            .shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    assertTrue(!!inputWrapper);

    assertEquals(0, getActiveObserversForTarget(testProxy.element).length);
    assertEquals(
        0, getActiveObserversForTarget(testProxy.element.$.matches).length);
    assertEquals(1, getActiveObserversForTarget(inputWrapper).length);
  });

  test('observeResize changes resync public resize observers', async () => {
    createComposeboxElement(testProxy, {observeResize: false});
    await flushComposebox();

    assertEquals(0, getActiveObserversForTarget(testProxy.element).length);
    assertEquals(
        0, getActiveObserversForTarget(testProxy.element.$.matches).length);

    testProxy.element.observeResize = true;
    await flushComposebox();

    assertEquals(1, getActiveObserversForTarget(testProxy.element).length);
    assertEquals(
        1, getActiveObserversForTarget(testProxy.element.$.matches).length);

    const composeboxObservers = [
      ...getObserversForTarget(testProxy.element),
      ...getObserversForTarget(testProxy.element.$.matches),
    ];

    testProxy.element.observeResize = false;
    await flushComposebox();

    assertEquals(0, getActiveObserversForTarget(testProxy.element).length);
    assertEquals(
        0, getActiveObserversForTarget(testProxy.element.$.matches).length);
    assertTrue(composeboxObservers.every(observer => observer.disconnected));
  });
});
