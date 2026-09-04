// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxElement, NtpComposeboxElement, SubmitButtonIconType} from 'chrome://new-tab-page/lazy_load.js';
import {$$, InputSource, QueryActionOverride} from 'chrome://new-tab-page/new_tab_page.js';
import {GlifAnimationState} from 'chrome://resources/cr_components/composebox/common.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxToolChipElement} from 'chrome://resources/cr_components/composebox/composebox_tool_chip.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {WindowProxy as CrWindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import type {SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {ADD_FILE_CONTEXT_FN, createComposeboxElement, getSubmitContainer, getSubmitIcon, MockInputState, setupComposeboxTest} from './test_support.js';

suite(`NewTabPageComposeboxTest`, () => {
  const testProxy = setupComposeboxTest();

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

    (testProxy.element.getInputElement().$.input as HTMLTextAreaElement).value =
        'test';
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

    (testProxy.element.getInputElement().$.input as HTMLTextAreaElement).value =
        'test';
    testProxy.element.getInputElement().$.input.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    const submitIcon = getSubmitIcon(testProxy);
    assertTrue(submitIcon.classList.contains('icon-arrow-upward'));
  });


  test(
      'submit disabled when tool is Deep Search (default entrypoint)',
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
        testProxy.searchboxCallbackRouterRemote.onInputStateChanged(inputState);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Assert button is still not in DOM.
        assertFalse(!!testProxy.element.shadowRoot.querySelector(
            'cr-composebox-submit'));
      });

  test(
      'clear functionality', async () => {
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
        (testProxy.element.getInputElement().$.input as HTMLTextAreaElement)
            .value = 'test';
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

  test(
      'suggestion activity link triggers navigation', async () => {
        createComposeboxElement(testProxy);
        await microtasksFinished();

        const matches = [
          createSearchMatchForTesting({
            isNoncannedAimSuggestion: true,
          }),
        ];
        testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              queryId: testProxy.element.activeQueryId,
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
              queryId: testProxy.element.activeQueryId,
              matches,
            }));
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        await testProxy.element.updateComplete;

        const suggestionActivity =
            testProxy.element.shadowRoot.querySelector('#suggestionActivity');
        assertFalse(!!suggestionActivity);
      });


  test(
      'cr-composebox-submit is not rendered when searchboxNextEnabled is true',
      async () => {
        createComposeboxElement(testProxy, {
          searchboxNextEnabled: true,
        });
        await microtasksFinished();

        const composeboxSubmit =
            testProxy.element.shadowRoot.querySelector('cr-composebox-submit');

        assertFalse(!!composeboxSubmit);
      });

  test(
      'cr-composebox-submit is rendered when searchboxLayoutMode is Compact',
      async () => {
        createComposeboxElement(testProxy, {
          searchboxNextEnabled: true,
        });
        testProxy.element.searchboxLayoutMode = 'Compact';
        (testProxy.element.getInputElement().$.input as HTMLTextAreaElement)
            .value = 'test';
        testProxy.element.getInputElement().$.input.dispatchEvent(
            new Event('input'));
        await microtasksFinished();

        const composeboxSubmit =
            testProxy.element.shadowRoot.querySelector('cr-composebox-submit');

        assertTrue(!!composeboxSubmit);
      });

  test(
      'cr-composebox-submit is not rendered when there is no input text',
      async () => {
        createComposeboxElement(testProxy, {
          searchboxNextEnabled: true,
        });
        testProxy.element.searchboxLayoutMode = 'Compact';
        (testProxy.element.getInputElement().$.input as HTMLTextAreaElement)
            .value = '';
        testProxy.element.getInputElement().$.input.dispatchEvent(
            new Event('input'));
        await microtasksFinished();

        const composeboxSubmit =
            testProxy.element.shadowRoot.querySelector('cr-composebox-submit');

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
    (testProxy.element.getInputElement().$.input as HTMLTextAreaElement).value =
        'test';
    testProxy.element.getInputElement().$.input.dispatchEvent(
        new Event('input'));
    const matches =
        [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: testProxy.element.activeQueryId,
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
    (testProxy.element.getInputElement().$.input as HTMLTextAreaElement).value =
        'test';
    testProxy.element.getInputElement().$.input.dispatchEvent(
        new Event('input'));
    const matches =
        [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: testProxy.element.activeQueryId,
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
    testProxy.element.getInputElement().$.input.dispatchEvent(shiftEnterEvent);
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

  test('Realbox keepMenuOpenOnTabSelect reads from loadTimeData', () => {
    loadTimeData.overrideValues({
      keepMenuOpenOnTabSelectForRealbox: false,
    });
    createComposeboxElement(testProxy);
    assertFalse(testProxy.element.keepMenuOpenOnTabSelect);

    loadTimeData.overrideValues({
      keepMenuOpenOnTabSelectForRealbox: true,
    });
    assertTrue(testProxy.element.keepMenuOpenOnTabSelect);
  });

  test('Realbox keepMenuOpenForMultiSelection gating behavior', async () => {
    createComposeboxElement(testProxy);

    let openMenuCalled = false;
    testProxy.element.getContextEntrypointElement = () => {
      return {
        openMenuForMultiSelection: () => {
          openMenuCalled = true;
        },
      } as ContextualEntrypointAndMenuElement;
    };

    // Case 1: `contextManagementEnabled`= true,
    // `keepMenuOpenOnTabSelectForRealbox` = false -> returns early
    // and does not keep menu open.
    testProxy.element.contextManagementInComposeboxEnabled = true;
    loadTimeData.overrideValues({
      keepMenuOpenOnTabSelectForRealbox: false,
    });
    openMenuCalled = false;
    await testProxy.element.keepMenuOpenForMultiSelection();
    assertFalse(openMenuCalled);

    // Case 2: `contextManagementEnabled = true`,
    // `keepMenuOpenOnTabSelectForRealbox = true` -> keeps menu open
    loadTimeData.overrideValues({
      keepMenuOpenOnTabSelectForRealbox: true,
    });
    openMenuCalled = false;
    await testProxy.element.keepMenuOpenForMultiSelection();
    assertTrue(openMenuCalled);

    // Case 3: `contextManagementEnabled = false` (!`contextMenu`) ->
    // keeps menu open regardless of `keepOpen` flag
    testProxy.element.contextManagementInComposeboxEnabled = false;
    loadTimeData.overrideValues({
      keepMenuOpenOnTabSelectForRealbox: false,
    });
    openMenuCalled = false;
    await testProxy.element.keepMenuOpenForMultiSelection();
    assertTrue(openMenuCalled);
  });

  test(
      'tool chip uses Clank layout for ImageGen and Canvas on Android',
      async () => {
        createComposeboxElement(testProxy, {
          searchboxNextEnabled: true,
        });
        testProxy.element.searchboxLayoutMode = 'Compact';
        testProxy.element.inToolMode = true;

        try {
          // Guard off: ImageGen renders the legacy layout.
          loadTimeData.overrideValues({
            isAndroid: false,
            useSearchboxConfigIconIds: false,
          });
          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              new MockInputState({activeTool: ToolMode.kImageGen}));
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          let chip = testProxy.element.shadowRoot
                         .querySelector<ComposeboxToolChipElement>(
                             '#toolChipsContainer cr-composebox-tool-chip');
          assertTrue(
              !!chip!.shadowRoot.querySelector('#leftCloseIcon'),
              'ImageGen should render the legacy layout when isAndroid is' +
                  ' false');
          assertEquals(
              'composebox:nanoBanana-custom',
              chip!.shadowRoot.querySelector<CrIconElement>('.tool-icon')!.icon,
              'ImageGen should keep the legacy banana icon when isAndroid is' +
                  ' false');

          // Guard on: ImageGen and Canvas render the Clank layout.
          loadTimeData.overrideValues({isAndroid: true});
          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              new MockInputState({activeTool: ToolMode.kImageGen}));
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          chip = testProxy.element.shadowRoot
                     .querySelector<ComposeboxToolChipElement>(
                         '#toolChipsContainer cr-composebox-tool-chip');
          assertTrue(
              !!chip!.shadowRoot.querySelector('.chip-close-icon'),
              'ImageGen should render the Clank close icon when isAndroid is' +
                  ' true');
          assertEquals(
              'composebox:nanoBanana-clank',
              chip!.shadowRoot
                  .querySelector<CrIconElement>('.chip-leading-icon')!.icon,
              'ImageGen should use the Clank banana icon when isAndroid is' +
                  ' true');

          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              new MockInputState({activeTool: ToolMode.kCanvas}));
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          chip = testProxy.element.shadowRoot
                     .querySelector<ComposeboxToolChipElement>(
                         '#toolChipsContainer cr-composebox-tool-chip');
          assertTrue(
              !!chip!.shadowRoot.querySelector('.chip-close-icon'),
              'Canvas should render the Clank close icon when isAndroid is' +
                  ' true');
        } finally {
          loadTimeData.overrideValues({
            isAndroid: false,
            useSearchboxConfigIconIds: true,
          });
        }
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

    test(
        'background is opaque when energy effect is enabled during voice search',
        async () => {
          // Enter voice search mode with static energy effect enabled.
          testProxy.element.energyEffectEnabled = true;
          testProxy.element.energyEffectAnimationEnabled = false;
          testProxy.element.isListening = true;
          testProxy.element.inVoiceSearchMode = true;
          await testProxy.element.updateComplete;

          const animatedGlow = testProxy.element.shadowRoot
                                   .querySelector<SearchAnimatedGlowElement>(
                                       'search-animated-glow');
          assertTrue(!!animatedGlow);
          await animatedGlow.updateComplete;

          // Verify that search-animated-glow preserves 'display: contents' and
          // isn't forced to 'display: block' / 'position: absolute'
          const computedStyle = window.getComputedStyle(animatedGlow);
          assertEquals(
              'contents', computedStyle.display,
              'search-animated-glow should be display: contents during voice search to preserve layout');
          assertNotEquals(
              'absolute', computedStyle.position,
              'search-animated-glow should not be absolute during voice search');
        });

    test(
        'voice search button tab order precedes cancel button' +
            ' and context entrypoint',
        () => {
          const composeboxInput =
              testProxy.element.shadowRoot.querySelector('cr-composebox-input');
          assertTrue(!!composeboxInput);

          const voiceSearchButton =
              testProxy.element.shadowRoot.querySelector('#voiceSearchButton');
          assertTrue(!!voiceSearchButton);
          assertEquals(
              'action-buttons', voiceSearchButton.getAttribute('slot'));

          const input = composeboxInput.shadowRoot.querySelector('#input');
          assertTrue(!!input);

          const actionButtonsSlot = composeboxInput.shadowRoot.querySelector(
              'slot[name="action-buttons"]');
          assertTrue(!!actionButtonsSlot);

          const cancelContainer =
              composeboxInput.shadowRoot.querySelector('#cancelContainer');
          assertTrue(!!cancelContainer);

          const contextEntrypoint =
              testProxy.element.shadowRoot.querySelector('#contextEntrypoint');
          assertTrue(!!contextEntrypoint);

          // Assert accessibility tabbing order:
          // Verify #input comes BEFORE actionButtonsSlot (Voice Search)
          assertTrue(
              (input.compareDocumentPosition(actionButtonsSlot) &
               Node.DOCUMENT_POSITION_FOLLOWING) !== 0);

          // Verify actionButtonsSlot (Voice Search) comes
          // BEFORE cancelContainer (Clear "X")
          assertTrue(
              (actionButtonsSlot.compareDocumentPosition(cancelContainer) &
               Node.DOCUMENT_POSITION_FOLLOWING) !== 0);

          // Verify composeboxInput (Voice Search + Clear)
          // comes BEFORE contextEntrypoint (+)
          assertTrue(
              (composeboxInput.compareDocumentPosition(contextEntrypoint) &
               Node.DOCUMENT_POSITION_FOLLOWING) !== 0);
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
          submitButton.shadowRoot.querySelector<HTMLElement>(
              '#submitContainer');
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
      const fileThumbnail =
          shadowRoot.querySelector('cr-composebox-file-thumbnail');
      assertTrue(!!fileThumbnail);
      const removeImgButton =
          fileThumbnail.shadowRoot.querySelector<HTMLElement>(
              '#removeImgButton');
      removeImgButton!.click();
      await microtasksFinished();
      await testProxy.element.updateComplete;
      assertEquals(0, testProxy.element.attachedContext.size);

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
      const fileThumbnail =
          shadowRoot.querySelector('cr-composebox-file-thumbnail');
      assertTrue(!!fileThumbnail);
      const removeImgButton =
          fileThumbnail.shadowRoot.querySelector<HTMLElement>(
              '#removeImgButton');
      removeImgButton!.click();
      await microtasksFinished();
      await testProxy.element.updateComplete;
      assertEquals(0, testProxy.element.attachedContext.size);

      // Submit:
      await submitVoiceSearch();

      assertTrue(testProxy.element.inToolMode);
      assertEquals(0, testProxy.element.attachedContext.size);
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
      assertEquals(1, testProxy.element.attachedContext.size);
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
          const fileThumbnail =
              shadowRoot.querySelector('cr-composebox-file-thumbnail');
          assertTrue(!!fileThumbnail);
          const removeImgButton =
              fileThumbnail.shadowRoot.querySelector<HTMLElement>(
                  '#removeImgButton');
          removeImgButton!.click();
          await microtasksFinished();
          await testProxy.element.updateComplete;
          assertEquals(0, testProxy.element.attachedContext.size);

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
          assertEquals(0, testProxy.element.attachedContext.size);
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
          voiceSearch.liveTranscriptEnabled = true;
          voiceSearch.isPermissionPromptOpen = false;
          await voiceSearch.updateComplete;

          const voiceSearchContainer =
              voiceSearch.shadowRoot.querySelector('#container');
          assertTrue(!!voiceSearchContainer);

          assertEquals(
              'absolute', window.getComputedStyle(voiceSearch).position);
          assertEquals(
              'relative',
              window.getComputedStyle(voiceSearchContainer).position);

          // Without live transcript:
          voiceSearch.liveTranscriptEnabled = false;
          await voiceSearch.updateComplete;
          assertEquals(
              'absolute',
              window.getComputedStyle(voiceSearchContainer).position);

          // Toggle back to true:
          voiceSearch.liveTranscriptEnabled = true;
          await voiceSearch.updateComplete;

          // Waiting (permission prompt open):
          voiceSearch.isPermissionPromptOpen = true;
          await voiceSearch.updateComplete;
          assertNotEquals(
              'absolute',
              window.getComputedStyle(voiceSearchContainer).position);

          // In error:
          voiceSearch.isPermissionPromptOpen = false;
          (voiceSearch as unknown as {errorMessage_: string}).errorMessage_ =
              'Voice error';
          await voiceSearch.updateComplete;
          assertNotEquals(
              'absolute',
              window.getComputedStyle(voiceSearchContainer).position);
        });
  });

  test('handleFuseboxAction applies and resets action state', async () => {
    const composebox = new NtpComposeboxElement();
    assertTrue(composebox.shouldHandleSuggestionFuseboxActions());
    const inputStateRequested =
        testProxy.searchboxHandler.whenCalled('getInputState');
    document.body.appendChild(composebox);
    await inputStateRequested;
    await microtasksFinished();

    await composebox.handleFuseboxAction({
      suggestion: 'paste suggestion',
      files: [],
      fuseboxAction: {
        preselectedTool: ToolMode.kDeepSearch,
        preferredInventory: SuggestInventory.kBrainstorm,
        preselectedModel: ModelMode.kGeminiPro,
        queryActionOverride: QueryActionOverride.kPaste,
        preselectedInputSource: null,
        searchboxOverride: null,
      },
    });
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals('paste suggestion', composebox.input);
    assertEquals(SuggestInventory.kBrainstorm, composebox.suggestInventory);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('setActiveToolMode'));
    assertEquals(
        ToolMode.kDeepSearch,
        testProxy.searchboxHandler.getArgs('setActiveToolMode')[0][0]);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('setActiveModelMode'));
    assertEquals(
        ModelMode.kGeminiPro,
        testProxy.searchboxHandler.getArgs('setActiveModelMode')[0][0]);

    await composebox.handleFuseboxAction({
      suggestion: 'second suggestion',
      files: [],
      fuseboxAction: {
        preselectedTool: null,
        preferredInventory: null,
        preselectedModel: null,
        queryActionOverride: QueryActionOverride.kPaste,
        preselectedInputSource: null,
        searchboxOverride: null,
      },
    });
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals('second suggestion', composebox.input);
    assertEquals(null, composebox.suggestInventory);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('setActiveToolMode'));
    assertEquals(
        2, testProxy.searchboxHandler.getCallCount('setActiveModelMode'));
    assertEquals(
        ModelMode.kUnspecified,
        testProxy.searchboxHandler.getArgs('setActiveModelMode')[1][0]);
  });

  test(
      'handleFuseboxAction triggers imageInput click for kInputSourceGallery',
      async () => {
        const composebox = new NtpComposeboxElement();
        document.body.appendChild(composebox);
        await microtasksFinished();

        let imageInputClicked = false;
        const imageInput =
            composebox.$.fileInputs.shadowRoot.querySelector<HTMLInputElement>(
                '#imageInput')!;
        imageInput.addEventListener('click', (e: Event) => {
          e.preventDefault();
          imageInputClicked = true;
        });

        await composebox.handleFuseboxAction({
          suggestion: '',
          files: [],
          fuseboxAction: {
            preselectedTool: null,
            preferredInventory: null,
            preselectedModel: null,
            queryActionOverride: null,
            preselectedInputSource: InputSource.kInputSourceGallery,
            searchboxOverride: null,
          },
        });
        await microtasksFinished();

        assertTrue(imageInputClicked);
      });

  test(
      'handleFuseboxAction triggers fileInput click for kInputSourceFilePicker',
      async () => {
        const composebox = new NtpComposeboxElement();
        document.body.appendChild(composebox);
        await microtasksFinished();

        let fileInputClicked = false;
        const fileInput =
            composebox.$.fileInputs.shadowRoot.querySelector<HTMLInputElement>(
                '#fileInput')!;
        fileInput.addEventListener('click', (e: Event) => {
          e.preventDefault();
          fileInputClicked = true;
        });

        await composebox.handleFuseboxAction({
          suggestion: '',
          files: [],
          fuseboxAction: {
            preselectedTool: null,
            preferredInventory: null,
            preselectedModel: null,
            queryActionOverride: null,
            preselectedInputSource: InputSource.kInputSourceFilePicker,
            searchboxOverride: null,
          },
        });
        await microtasksFinished();

        assertTrue(fileInputClicked);
      });

  test(
      'getFileInputsElement returns element or null when disabled',
      async () => {
        const composebox = new NtpComposeboxElement();
        composebox.contextMenuEnabled = true;
        document.body.appendChild(composebox);
        await microtasksFinished();

        assertEquals(
            composebox.$.fileInputs, composebox.getFileInputsElement());
        composebox.contextMenuEnabled = false;
        assertEquals(null, composebox.getFileInputsElement());
      });

  test(
      'handleFuseboxAction opens tab picker for kInputSourceTabPicker',
      async () => {
        const composebox = new NtpComposeboxElement();
        composebox.contextMenuEnabled = true;
        document.body.appendChild(composebox);
        await microtasksFinished();

        await composebox.handleFuseboxAction({
          suggestion: '',
          files: [],
          fuseboxAction: {
            preselectedTool: null,
            preferredInventory: null,
            preselectedModel: null,
            queryActionOverride: null,
            preselectedInputSource: InputSource.kInputSourceTabPicker,
            searchboxOverride: null,
          },
        });
        await microtasksFinished();

        assertTrue(composebox.shareTabsFlyoutOpen);
      });

  test(
      'handleFuseboxAction triggers voice search for kInputSourceVoice',
      async () => {
        const composebox = new NtpComposeboxElement();
        document.body.appendChild(composebox);
        await microtasksFinished();

        let voiceSearchClicked = false;
        composebox.onVoiceSearchButtonClick = () => {
          voiceSearchClicked = true;
        };

        await composebox.handleFuseboxAction({
          suggestion: '',
          files: [],
          fuseboxAction: {
            preselectedTool: null,
            preferredInventory: null,
            preselectedModel: null,
            queryActionOverride: null,
            preselectedInputSource: InputSource.kInputSourceVoice,
            searchboxOverride: null,
          },
        });
        await microtasksFinished();

        assertTrue(voiceSearchClicked);
      });

  test(
      'hint action sets the placeholder and survives input state updates',
      async () => {
        const composebox = new NtpComposeboxElement();
        document.body.appendChild(composebox);
        await microtasksFinished();
        await composebox.updateComplete;
        await composebox.getInputElement().updateComplete;
        const input = composebox.getInputElement().$.input;

        await composebox.handleFuseboxAction({
          suggestion: 'chip hint',
          files: [],
          fuseboxAction: {
            preselectedTool: null,
            preferredInventory: null,
            preselectedModel: null,
            queryActionOverride: QueryActionOverride.kHint,
            preselectedInputSource: null,
            searchboxOverride: null,
          },
        });
        await composebox.updateComplete;
        await composebox.getInputElement().updateComplete;
        assertEquals('chip hint', input.getAttribute('placeholder'));

        // An asynchronous input state update carrying its own hint must not
        // clobber the active chip hint.
        testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
            new MockInputState({hintText: 'server hint'}));
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();
        await composebox.updateComplete;
        await composebox.getInputElement().updateComplete;
        assertEquals('chip hint', input.getAttribute('placeholder'));
      });

  // TODO(crbug.com/548681676): Verify that actions trigger the contextual
  // entrypoint energy effect animation only when animation and test mode are
  // enabled. Update to test TutorialId once the server proto rolls.
  [false, true].forEach(scaledActionChipsInTestMode => {
    [false, true].forEach(energyEffectAnimationEnabled => {
      test(
          `handleFuseboxAction animation with testMode=${
              scaledActionChipsInTestMode}, energyEnabled=${
              energyEffectAnimationEnabled}`,
          async () => {
            loadTimeData.overrideValues({scaledActionChipsInTestMode});
            const composebox = new NtpComposeboxElement();
            composebox.energyEffectAnimationEnabled =
                energyEffectAnimationEnabled;
            document.body.appendChild(composebox);
            await microtasksFinished();

            const action = {
              preselectedTool: ToolMode.kUnspecified,
              preferredInventory: null,
              preselectedModel: null,
              queryActionOverride: null,
              preselectedInputSource: null,
              searchboxOverride: null,
            };

            const expectedState =
                scaledActionChipsInTestMode && energyEffectAnimationEnabled ?
                GlifAnimationState.STARTED :
                GlifAnimationState.INELIGIBLE;
            await composebox.handleFuseboxAction({
              suggestion: '',
              files: [],
              fuseboxAction: action,
            });
            await new Promise(resolve => requestAnimationFrame(resolve));
            assertEquals(expectedState, composebox.glifAnimationState);
          });
    });
  });
});

// ==========================================================
// RESIZE OBSERVER SUITE
// ==========================================================
// TODO(crbug.com/535685540): Remove this suite and its tests from here once
// `cr-composebox` element is no longer used.
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
        testProxy.element = new ComposeboxElement();
        Object.assign(testProxy.element, {observeResize: true});
        document.body.appendChild(testProxy.element);
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
    testProxy.element = new ComposeboxElement();
    Object.assign(testProxy.element, {observeResize: false});
    document.body.appendChild(testProxy.element);
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
    testProxy.element = new ComposeboxElement();
    Object.assign(testProxy.element, {observeResize: false});
    document.body.appendChild(testProxy.element);
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

  test(
      'smartTabSharingActive causes hasTabs true and input has has-tabs class',
      async () => {
        testProxy.searchboxHandler.setPromiseResolveFor(
            'getSmartTabSharingActive', {active: true});
        createComposeboxElement(testProxy, {
          searchboxNextEnabled: true,
          smartTabSharingActive: true,
          smartTabSharingVisible: true,
        });
        await microtasksFinished();
        await testProxy.element.updateComplete;

        assertTrue(testProxy.element.hasTabs());
        assertFalse(testProxy.element.hasAttribute('should-remain-folded_'));

        const inputElement = testProxy.element.getInputElement();
        assertTrue(inputElement.classList.contains('has-tabs'));
      });
});
