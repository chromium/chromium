// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SubmitButtonIconType} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {ContextType, ContextualSearchInputStateDeletionType} from 'chrome://resources/cr_components/composebox/common.js';
import {ModelMode, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
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

  test('submit button is a no-op when disabled', async () => {
    createComposeboxElement(testProxy);
    assertEquals(testProxy.searchboxHandler.getCallCount('submitQuery'), 0);
    assertEquals(
        testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

    // Arrange.
    testProxy.element.getInputElement().$.input.value = '';
    testProxy.element.getInputElement().$.input.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    // Assert submit is disabled.
    const submitButton = getSubmitIcon(testProxy);
    assertTrue(submitButton.hasAttribute('disabled'));

    // Act.
    getSubmitContainer(testProxy).click();
    await microtasksFinished();

    // Assert no calls were made.
    assertEquals(testProxy.searchboxHandler.getCallCount('submitQuery'), 0);
    assertEquals(
        testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 0);
  });

  test('empty input has disabled submit button', async () => {
    createComposeboxElement(testProxy);

    // Arrange.
    testProxy.element.getInputElement().$.input.value = '';
    testProxy.element.getInputElement().$.input.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    // Assert call cannot occur.
    const submitButton = getSubmitIcon(testProxy);
    assertTrue(submitButton.hasAttribute('disabled'));
  });

  test('submit button is disabled', async () => {
    // Arrange.
    testProxy.element.getInputElement().$.input.value = ' ';
    testProxy.element.getInputElement().$.input.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    // Assert.
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
    const whenCloseComposebox =
        eventToPromise<CustomEvent<{composeboxText: string}>>(
            'close-composebox', testProxy.element);
    $$<HTMLElement>(
        testProxy.element.getInputElement(), '#cancelIcon')!.click();
    await whenCloseComposebox;
    assertEquals(testProxy.searchboxHandler.getCallCount('clearFiles'), 2);
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
    }

    test('updates state from state property', async () => {
      createComposeboxElement(testProxy);
      testProxy.searchboxHandler.setPromiseResolveFor(
          ADD_FILE_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});
      const composebox = testProxy.element;

      composebox.state = {
        text: 'hello world',
        files: [
          {file: new File(['test'], 'test.pdf', {type: 'application/pdf'})},
        ],
        mode: ToolMode.kDeepSearch,
        model: ModelMode.kGeminiRegular,
      };
      await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
      await composebox.updateComplete;
      await microtasksFinished();

      assertEquals('hello world', composebox.input);
      const activeTool =
          await testProxy.searchboxHandler.whenCalled('setActiveToolMode');
      assertEquals(ToolMode.kDeepSearch, activeTool);
      assertEquals(1, composebox.files.size);
      const activeModel =
          await testProxy.searchboxHandler.whenCalled('setActiveModelMode');
      assertEquals(ModelMode.kGeminiRegular, activeModel);
    });

    if (useForked) {
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

    test('ShowContextMenuDescription', async () => {
      loadTimeData.overrideValues({
        composeboxShowContextMenuDescription: false,
      });
      createComposeboxElement(testProxy);
      await microtasksFinished();

      let entrypoint = $$(testProxy.element, '#contextEntrypoint');
      assertTrue(!!entrypoint);
      assertFalse(entrypoint.hasAttribute('show-context-menu-description'));

      testProxy.element.remove();

      loadTimeData.overrideValues({
        composeboxShowContextMenuDescription: true,
      });
      createComposeboxElement(testProxy);
      await microtasksFinished();

      entrypoint = $$(testProxy.element, '#contextEntrypoint');
      assertTrue(!!entrypoint);
      assertTrue(entrypoint.hasAttribute('show-context-menu-description'));
    });

    test('metrics are recorded for ToolMode clicks', async () => {
      loadTimeData.overrideValues({composeboxSource: 'NewTabPage'});
      createComposeboxElement(testProxy);
      await microtasksFinished();

      const composebox = testProxy.element;
      const entrypointAndMenu = $$(composebox, '#contextEntrypoint');
      assertTrue(!!entrypointAndMenu);

      const metricName =
          'NewTabPage.AimEntrypoint.AimPopup.ContextualElement.Clicked';

      // Act: DeepSearch
      entrypointAndMenu.dispatchEvent(new CustomEvent('tool-click', {
        detail: {toolMode: ToolMode.kDeepSearch},
      }));
      assertEquals(
          1, testProxy.metrics.count(metricName, ContextType.DEEP_RESEARCH));
      assertEquals(1, testProxy.metrics.count(`${metricName}.DeepResearch`, 0));

      // Act: ImageGen
      entrypointAndMenu.dispatchEvent(new CustomEvent('tool-click', {
        detail: {toolMode: ToolMode.kImageGen},
      }));
      assertEquals(
          1, testProxy.metrics.count(metricName, ContextType.IMAGE_GEN));
      assertEquals(1, testProxy.metrics.count(`${metricName}.ImageGen`, 0));

      // Act: Canvas
      entrypointAndMenu.dispatchEvent(new CustomEvent('tool-click', {
        detail: {toolMode: ToolMode.kCanvas},
      }));
      assertEquals(1, testProxy.metrics.count(metricName, ContextType.CANVAS));
      assertEquals(1, testProxy.metrics.count(`${metricName}.Canvas`, 0));
    });

    test('metrics are recorded for ModelMode clicks', async () => {
      loadTimeData.overrideValues({composeboxSource: 'NewTabPage'});
      createComposeboxElement(testProxy);
      await microtasksFinished();

      const composebox = testProxy.element;
      const entrypointAndMenu = $$(composebox, '#contextEntrypoint');
      assertTrue(!!entrypointAndMenu);

      const metricName =
          'NewTabPage.AimEntrypoint.AimPopup.ContextualElement.Clicked';

      // Act: Auto
      entrypointAndMenu.dispatchEvent(new CustomEvent('model-click', {
        detail: {model: ModelMode.kGeminiProAutoroute},
      }));
      assertEquals(
          1, testProxy.metrics.count(metricName, ContextType.AUTO_MODEL));
      assertEquals(1, testProxy.metrics.count(`${metricName}.AutoModel`, 0));

      // Act: Thinking
      entrypointAndMenu.dispatchEvent(new CustomEvent('model-click', {
        detail: {model: ModelMode.kGeminiPro},
      }));
      assertEquals(
          1, testProxy.metrics.count(metricName, ContextType.THINKING_MODEL));
      assertEquals(
          1, testProxy.metrics.count(`${metricName}.ThinkingModel`, 0));

      // Act: Regular
      entrypointAndMenu.dispatchEvent(new CustomEvent('model-click', {
        detail: {model: ModelMode.kGeminiRegular},
      }));
      assertEquals(
          1, testProxy.metrics.count(metricName, ContextType.REGULAR_MODEL));
      assertEquals(1, testProxy.metrics.count(`${metricName}.RegularModel`, 0));

      // Act: ProNoGenUi
      entrypointAndMenu.dispatchEvent(new CustomEvent('model-click', {
        detail: {model: ModelMode.kGeminiProNoGenUi},
      }));
      assertEquals(
          1,
          testProxy.metrics.count(metricName, ContextType.PRO_NO_GEN_UI_MODEL));
      assertEquals(
          1, testProxy.metrics.count(`${metricName}.ProNoGenUiModel`, 0));
    });

    test('metrics are recorded for file uploads', async () => {
      loadTimeData.overrideValues({composeboxSource: 'NewTabPage'});
      createComposeboxElement(testProxy);
      await microtasksFinished();

      const composebox = testProxy.element;
      const entrypointAndMenu = $$(composebox, '#contextEntrypoint');
      assertTrue(!!entrypointAndMenu);

      const metricName =
          'NewTabPage.AimEntrypoint.AimPopup.ContextualElement.Clicked';

      // Act: Upload an image file from the context menu
      entrypointAndMenu.dispatchEvent(new CustomEvent('open-image-upload'));
      assertEquals(1, testProxy.metrics.count(metricName, ContextType.IMAGE));
      assertEquals(1, testProxy.metrics.count(`${metricName}.Image`, 0));

      // Act: Upload a regular file
      entrypointAndMenu.dispatchEvent(new CustomEvent('open-file-upload'));
      assertEquals(1, testProxy.metrics.count(metricName, ContextType.FILE));
      assertEquals(1, testProxy.metrics.count(`${metricName}.File`, 0));
    });

    test('metrics are recorded for tab additions', async () => {
      loadTimeData.overrideValues({composeboxSource: 'NewTabPage'});
      createComposeboxElement(testProxy);
      await microtasksFinished();

      const composebox = testProxy.element;
      const entrypointAndMenu = $$(composebox, '#contextEntrypoint');
      assertTrue(!!entrypointAndMenu);

      const metricName =
          'NewTabPage.AimEntrypoint.AimPopup.ContextualElement.Clicked';

      entrypointAndMenu.dispatchEvent(new CustomEvent('add-tab-context', {
        detail: {
          id: 1,
          title: 'Title',
          url: {url: 'http://test.com'},
          delayUpload: false,
          origin: 0,
        },
      }));
      assertEquals(1, testProxy.metrics.count(metricName, ContextType.TAB));
      assertEquals(1, testProxy.metrics.count(`${metricName}.Tab`, 0));
    });

    test('session abandoned on cancel button click', async () => {
      // Arrange.
      createComposeboxElement(testProxy);

      await microtasksFinished();

      // Close composebox.
      const whenCloseComposebox =
          eventToPromise<CustomEvent<{composeboxText: string}>>(
              'close-composebox', testProxy.element);
      const cancelIcon =
          $$<HTMLElement>(testProxy.element.getInputElement(), '#cancelIcon');
      cancelIcon!.click();
      const event = await whenCloseComposebox;
      assertEquals(event.detail.composeboxText, '');
      assertEquals(testProxy.searchboxHandler.getCallCount('clearFiles'), 1);
    });

    test('NotifySessionStarted called on composebox created', () => {
      // Assert call has not occurred.
      assertEquals(
          testProxy.searchboxHandler.getCallCount('notifySessionStarted'), 0);

      createComposeboxElement(testProxy);

      // Assert call occurs.
      assertEquals(
          testProxy.searchboxHandler.getCallCount('notifySessionStarted'), 1);
    });

    test('clear button title changes with input', async () => {
      createComposeboxElement(testProxy);
      assertEquals(
          testProxy.element.getInputElement().$.cancelIcon.getAttribute(
              'title'),
          loadTimeData.getString('composeboxCancelButtonTitle'));
      // Arrange.
      testProxy.element.getInputElement().$.input.value = 'Test';
      testProxy.element.getInputElement().$.input.dispatchEvent(
          new Event('input'));
      await microtasksFinished();

      // Assert.
      assertEquals(
          testProxy.element.getInputElement().$.cancelIcon.getAttribute(
              'title'),
          loadTimeData.getString('composeboxCancelButtonTitleInput'));
    });

    test('Smart Compose hint is hidden during backspacing', async () => {
      // Enable Smart Compose.
      loadTimeData.overrideValues({composeboxSmartComposeEnabled: true});
      createComposeboxElement(testProxy);
      const inputElement = testProxy.element.getInputElement();
      const input = inputElement.$.input;

      // Provide an input and a hint.
      input.value = 'tes';
      input.dispatchEvent(new Event('input'));
      const hint = 't';

      testProxy.element.haveReceivedSynchronousAutocompleteResponse = true;
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            input: 'tes',
            smartComposeInlineHint: hint,
          }));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Verify hint is visible.
      assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));

      // Simulate backspace.
      input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Backspace'}));
      await microtasksFinished();

      // Verify hint is hidden.
      assertFalse(!!inputElement.shadowRoot.querySelector('#smartCompose'));

      // Simulate typing a character.
      input.dispatchEvent(new KeyboardEvent('keydown', {key: 'a'}));
      await microtasksFinished();

      // Verify hint is visible again.
      assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));
    });

    test(
        'Smart Compose hint is hidden when it wraps in the middle of a word',
        async () => {
          // Enable Smart Compose.
          loadTimeData.overrideValues({composeboxSmartComposeEnabled: true});
          createComposeboxElement(testProxy);
          const inputElement = testProxy.element.getInputElement();
          const input = inputElement.$.input;

          // Mock Canvas measureText and clientWidth.
          const originalMeasureText =
              CanvasRenderingContext2D.prototype.measureText;
          CanvasRenderingContext2D.prototype.measureText = function(
              text: string) {
            if (text.includes('wrap')) {
              return {width: 150} as TextMetrics;
            }
            return {width: 50} as TextMetrics;
          };
          Object.defineProperty(
              input, 'clientWidth', {configurable: true, get: () => 100});

          // Provide an input ending with a non-space character and a hint.
          input.value = 'tes.';
          input.dispatchEvent(new Event('input'));
          const hint = 'wrap';  // This will trigger width = 150

          testProxy.element.haveReceivedSynchronousAutocompleteResponse = true;
          testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                input: 'tes.',
                smartComposeInlineHint: hint,
              }));
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          // Trigger re-evaluation by requesting update.
          inputElement.requestUpdate();
          await microtasksFinished();

          // Verify hint is hidden.
          assertFalse(!!inputElement.shadowRoot.querySelector('#smartCompose'));

          // Restore mock.
          CanvasRenderingContext2D.prototype.measureText = originalMeasureText;
        });

    test(
        'Smart Compose hint is NOT hidden when only full hint wraps but first word fits',
        async () => {
          // Enable Smart Compose.
          loadTimeData.overrideValues({composeboxSmartComposeEnabled: true});
          createComposeboxElement(testProxy);
          const inputElement = testProxy.element.getInputElement();
          const input = inputElement.$.input;

          // Mock Canvas measureText and clientWidth.
          const originalMeasureText =
              CanvasRenderingContext2D.prototype.measureText;
          CanvasRenderingContext2D.prototype.measureText = function(
              text: string) {
            if (text.includes('wraps')) {
              return {width: 150} as TextMetrics;
            }
            return {width: 50} as TextMetrics;
          };
          Object.defineProperty(
              input, 'clientWidth', {configurable: true, get: () => 100});

          // Provide an input ending with a non-space character and a hint.
          input.value = 'tes.';
          input.dispatchEvent(new Event('input'));
          const hint = 'fits wraps';

          testProxy.element.haveReceivedSynchronousAutocompleteResponse = true;
          testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                input: 'tes.',
                smartComposeInlineHint: hint,
              }));
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          // Trigger re-evaluation by requesting update.
          inputElement.requestUpdate();
          await microtasksFinished();

          // Verify hint is visible.
          assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));

          // Restore mock.
          CanvasRenderingContext2D.prototype.measureText = originalMeasureText;
        });

    test(
        'Smart Compose hint is hidden when cursor is not at the end',
        async () => {
          // Enable Smart Compose.
          loadTimeData.overrideValues({composeboxSmartComposeEnabled: true});
          createComposeboxElement(testProxy);
          const inputElement = testProxy.element.getInputElement();
          const input = inputElement.$.input;

          // Provide an input and a hint.
          input.value = 'test';
          input.dispatchEvent(new Event('input'));
          const hint = 'a';

          testProxy.element.haveReceivedSynchronousAutocompleteResponse = true;
          testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                input: 'test',
                smartComposeInlineHint: hint,
              }));
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          // Verify hint is visible initially.
          assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));

          // Move cursor to the middle.
          input.selectionStart = 2;
          input.selectionEnd = 2;

          // Trigger re-evaluation.
          inputElement.requestUpdate();
          await microtasksFinished();

          // Verify hint is hidden.
          assertFalse(!!inputElement.shadowRoot.querySelector('#smartCompose'));
        });

    test('onInputStateChanged updates inputState', async () => {
      createComposeboxElement(testProxy);
      const inputState = {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        inputTypeConfigs: [],
        toolConfigs: [],
        modelConfigs: [],
        toolsSectionConfig: null,
        modelSectionConfig: null,
        hintText: '',
        maxInputsByType: {},
        maxTotalInputs: 0,
        isCanvasQuerySubmitted: false,
      } as InputState;
      testProxy.searchboxCallbackRouterRemote.onInputStateChanged(inputState);
      await microtasksFinished();
      assertDeepEquals(testProxy.element.inputState, inputState);
    });

    test('setDefaultModel uses activeModel from backend', async () => {
      createComposeboxElement(testProxy);

      const inputState = new MockInputState({
        allowedModels: [ModelMode.kGeminiRegular, ModelMode.kGeminiPro],
        activeModel: ModelMode.kGeminiPro,
        modelConfigs: [
          {
            model: ModelMode.kGeminiRegular,
            aimUrlParams: [],
            menuLabel: 'Regular',
            hintText: 'Hint Regular',
          },
          {
            model: ModelMode.kGeminiPro,
            aimUrlParams: [{paramKey: 'xyz', paramValue: '1'}],
            menuLabel: 'Pro',
            hintText: 'Hint Pro',
          },
        ],
        modelSectionConfig: null,
      });

      testProxy.searchboxCallbackRouterRemote.onInputStateChanged(inputState);
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      testProxy.element.setDefaultModel();

      assertEquals(
          testProxy.searchboxHandler.getCallCount('setActiveModelMode'), 1);
      const arg = testProxy.searchboxHandler.getArgs('setActiveModelMode')[0];
      assertEquals(arg, ModelMode.kGeminiPro);
    });

    test('navigates matches with ArrowDown and ArrowUp', async () => {
      createComposeboxElement(testProxy);
      const input = testProxy.element.getInputElement().$.input;
      const matchesElement = testProxy.element.$.matches;

      // Verify navigation is blocked when no matches are available.
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({matches: []}));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(-1, matchesElement.selectedMatchIndex);

      // Populate matches for testing.
      const matches = [
        createSearchMatchForTesting({fillIntoEdit: 'test1'}),
        createSearchMatchForTesting({fillIntoEdit: 'test2'}),
      ];
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({matches}));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Verify navigation is blocked when focus is in input but dropdown is
      // hidden.
      input.focus();
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({matches: []}));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(-1, matchesElement.selectedMatchIndex);

      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({matches}));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Verify navigation is blocked when key modifiers are present.
      input.dispatchEvent(new KeyboardEvent(
          'keydown',
          {key: 'ArrowDown', ctrlKey: true, bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(-1, matchesElement.selectedMatchIndex);

      // Verify normal navigation when all guard conditions are met.
      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(0, matchesElement.selectedMatchIndex);

      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(1, matchesElement.selectedMatchIndex);

      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'ArrowUp', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(0, matchesElement.selectedMatchIndex);
    });

    test('selects first or last match with PageUp and PageDown', async () => {
      createComposeboxElement(testProxy);
      const input = testProxy.element.getInputElement().$.input;
      const matchesElement = testProxy.element.$.matches;

      // Verify navigation is blocked when no matches are available.
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({matches: []}));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'PageDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(-1, matchesElement.selectedMatchIndex);

      const matches = [
        createSearchMatchForTesting({fillIntoEdit: 'test1'}),
        createSearchMatchForTesting({fillIntoEdit: 'test2'}),
        createSearchMatchForTesting({fillIntoEdit: 'test3'}),
      ];
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({matches}));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Verify navigation is blocked when key modifiers are present.
      input.dispatchEvent(new KeyboardEvent(
          'keydown',
          {key: 'PageDown', altKey: true, bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(-1, matchesElement.selectedMatchIndex);

      // Verify navigation to the last and first match.
      // PageDown selects the last match.
      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'PageDown', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(2, matchesElement.selectedMatchIndex);

      // PageUp selects the first match.
      input.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'PageUp', bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(0, matchesElement.selectedMatchIndex);
    });

    test('Tab behavior when focus is in input', async () => {
      loadTimeData.overrideValues({composeboxSmartComposeEnabled: true});
      createComposeboxElement(testProxy);
      const input = testProxy.element.getInputElement().$.input;
      const matchesElement = testProxy.element.$.matches;

      // Populate matches and select the first one.
      const matches = [createSearchMatchForTesting()];
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({matches}));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

      matchesElement.selectNext();
      assertEquals(0, matchesElement.selectedMatchIndex);

      // Ensure focus is in the input.
      input.focus();

      // Verify Shift+Tab unselects the match.
      input.dispatchEvent(new KeyboardEvent(
          'keydown',
          {key: 'Tab', shiftKey: true, bubbles: true, composed: true}));
      await microtasksFinished();
      assertEquals(-1, matchesElement.selectedMatchIndex);

      // Verify Tab accepts the Smart Compose hint when available.
      input.value = 'tes';
      input.dispatchEvent(new Event('input'));
      const hint = 't';

      testProxy.element.haveReceivedSynchronousAutocompleteResponse = true;
      testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResultForTesting({
            input: 'tes',
            matches,
            smartComposeInlineHint: hint,
          }));
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

      const tabEvent = new KeyboardEvent(
          'keydown',
          {key: 'Tab', bubbles: true, cancelable: true, composed: true});
      input.dispatchEvent(tabEvent);
      await microtasksFinished();

      assertEquals('test', input.value);
      assertTrue(tabEvent.defaultPrevented);
    });

    test(
        'Tab behavior in matches list bypasses input focus check', async () => {
          createComposeboxElement(testProxy);
          const input = testProxy.element.getInputElement().$.input;
          const matchesElement = testProxy.element.$.matches;

          // Move focus away from the input so it bypasses input focus check.
          input.blur();
          matchesElement.focus();
          assertTrue(input !== testProxy.element.shadowRoot.activeElement);

          // Verify Tab is ignored when no matches are available.
          testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({matches: []}));
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

          const emptyEvent = new KeyboardEvent(
              'keydown',
              {key: 'Tab', bubbles: true, cancelable: true, composed: true});
          matchesElement.dispatchEvent(emptyEvent);
          await microtasksFinished();
          assertFalse(emptyEvent.defaultPrevented);

          // Populate matches.
          const matches = [
            createSearchMatchForTesting(
                {fillIntoEdit: 'match1', supportsDeletion: false}),
            createSearchMatchForTesting(
                {fillIntoEdit: 'match2', supportsDeletion: false}),
          ];
          testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({matches}));
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          // Verify Tab is ignored when modifiers are present.
          const modifierEvent = new KeyboardEvent('keydown', {
            key: 'Tab',
            ctrlKey: true,
            bubbles: true,
            cancelable: true,
            composed: true,
          });
          matchesElement.dispatchEvent(modifierEvent);
          await microtasksFinished();
          assertFalse(modifierEvent.defaultPrevented);

          // Select the last match.
          input.focus();
          input.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'ArrowUp', bubbles: true, composed: true}));
          await microtasksFinished();
          assertEquals(1, matchesElement.selectedMatchIndex);

          // Move focus back to matches element to bypass the input block.
          input.blur();
          matchesElement.focus();
          assertTrue(input !== testProxy.element.shadowRoot.activeElement);

          // Verify normal Tab behavior unselects the last match.
          const normalTabEvent = new KeyboardEvent(
              'keydown',
              {key: 'Tab', bubbles: true, cancelable: true, composed: true});
          matchesElement.dispatchEvent(normalTabEvent);
          await microtasksFinished();

          // Verify the match is unselected.
          assertEquals(-1, matchesElement.selectedMatchIndex);
          // Default behavior is not prevented, allowing focus to move.
          assertFalse(normalTabEvent.defaultPrevented);
        });

    test('session abandoned on esc click', async () => {
      // Arrange.
      createComposeboxElement(testProxy, {closeOnEscape: true});

      testProxy.element.getInputElement().$.input.value = 'test';
      testProxy.element.getInputElement().$.input.dispatchEvent(
          new Event('input'));
      await microtasksFinished();

      const whenCloseComposebox =
          eventToPromise<CustomEvent<{composeboxText: string}>>(
              'close-composebox', testProxy.element);

      // Assert call occurs.
      testProxy.element.$.composebox.dispatchEvent(
          new KeyboardEvent('keydown', {key: 'Escape'}));
      await microtasksFinished();
      const event = await whenCloseComposebox;
      assertEquals(event.detail.composeboxText, 'test');
      assertEquals(testProxy.searchboxHandler.getCallCount('clearFiles'), 1);
    });

    test(
        'esc clears input instead of closing when closeOnEscape is false and has content',
        async () => {
          // Arrange.
          createComposeboxElement(testProxy, {closeOnEscape: false});

          testProxy.element.getInputElement().$.input.value = 'test';
          testProxy.element.getInputElement().$.input.dispatchEvent(
              new Event('input'));
          await microtasksFinished();

          const closePromise =
              eventToPromise('close-composebox', testProxy.element);
          let closed = false;
          closePromise.then(() => closed = true);

          // Act
          testProxy.element.$.composebox.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Escape'}));
          await microtasksFinished();

          // Assert: the clear branch fired instead of close-composebox.
          assertFalse(closed);
          assertEquals('', testProxy.element.getInputElement().$.input.value);
          assertEquals(
              testProxy.searchboxHandler.getCallCount('clearFiles'), 1);
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

    test('set and delete visual selection thumbnail', async () => {
      createComposeboxElement(testProxy);
      await microtasksFinished();

      // Initially, carousel is not shown.
      assertFalse(testProxy.element.hasAttribute('show-file-carousel'));

      // Set a thumbnail.
      const thumbnailUrl = 'data:image/png;base64,sometestdata';
      testProxy.searchboxCallbackRouterRemote.addFileContext(
          FAKE_TOKEN_STRING, {
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

      assertEquals(fileCarousel.files.length, 1);
      assertDeepEquals(fileCarousel.files[0]!.uuid, FAKE_TOKEN_STRING);
      assertEquals(fileCarousel.files[0]!.dataUrl, thumbnailUrl);
      assertTrue(fileCarousel.files[0]!.isDeletable);

      // Delete the thumbnail.
      const fileThumbnail =
          fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
      assertTrue(!!fileThumbnail);

      const removeImgButton =
          fileThumbnail.shadowRoot.querySelector<HTMLElement>(
              '#removeImgButton');
      removeImgButton!.click();
      await microtasksFinished();

      // Assert thumbnail is removed.
      assertEquals(testProxy.searchboxHandler.getCallCount('deleteContext'), 1);
      const [idArg, fromChip] =
          testProxy.searchboxHandler.getArgs('deleteContext')[0];
      assertEquals(idArg, FAKE_TOKEN_STRING);
      assertFalse(fromChip);
      // The carousel is removed from the DOM when there are no files, so
      // assert its absence.
      assertFalse(!!testProxy.element.shadowRoot.querySelector('#carousel'));
      assertFalse(testProxy.element.hasAttribute('show-file-carousel'));
    });

    test('setVisualSelectionThumbnail not deletable', async () => {
      createComposeboxElement(testProxy);
      await microtasksFinished();

      // Set a thumbnail that is not deletable.
      const thumbnailUrl = 'data:image/png;base64,sometestdata';
      testProxy.searchboxCallbackRouterRemote.addFileContext(
          FAKE_TOKEN_STRING, {
            fileName: 'Visual Selection',
            mimeType: 'image/png',
            imageDataUrl: thumbnailUrl,
            isDeletable: false,
            selectionTime: new Date(),
          } as SelectedFileInfo);
      await microtasksFinished();

      // Assert thumbnail is shown.
      assertTrue(testProxy.element.hasAttribute('show-file-carousel'));
      const fileCarousel = testProxy.element.$.carousel;
      assertEquals(fileCarousel.files.length, 1);
      assertFalse(fileCarousel.files[0]!.isDeletable);

      // Assert delete button is not present.
      const fileThumbnail =
          fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
      assertTrue(!!fileThumbnail);
      const removeButton = fileThumbnail.shadowRoot.querySelector<HTMLElement>(
          '#removeImgButton');
      assertEquals(null, removeButton);
    });

    test('delete tool chip', async () => {
      loadTimeData.overrideValues({composeboxSource: 'NewTabPage'});
      createComposeboxElement(testProxy);
      await microtasksFinished();

      // Set active tool mode to DeepSearch.
      const inputState = new MockInputState({
        activeTool: ToolMode.kDeepSearch,
      });
      testProxy.searchboxCallbackRouterRemote.onInputStateChanged(inputState);
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Click on the same tool mode to deselect/delete it.
      testProxy.element.handleToolClick(ToolMode.kDeepSearch);
      await microtasksFinished();

      // Assert tool mode is reset.
      const activeTool =
          await testProxy.searchboxHandler.whenCalled('setActiveToolMode');
      assertEquals(ToolMode.kUnspecified, activeTool);

      const metricName =
          'ContextualSearch.UserAction.InputStateDeletion.NewTabPage';
      assertEquals(
          1,
          testProxy.metrics.count(
              metricName, ContextualSearchInputStateDeletionType.TOOL));
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
