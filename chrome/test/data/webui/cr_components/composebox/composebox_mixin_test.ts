// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox_dropdown.js';
import 'chrome://resources/cr_components/composebox/composebox_file_inputs.js';
import 'chrome://resources/cr_components/composebox/composebox_input.js';
import 'chrome://resources/cr_components/composebox/file_carousel.js';

import {ComposeboxFile, ContextType, ContextualSearchInputStateDeletionType, TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from 'chrome://resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from 'chrome://resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {DriveDisclaimerStatus, DriveUploadError, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch, AutocompleteResult, PageRemote as SearchboxPageRemote, SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock, MockInputState} from './composebox_test_utils.js';

const TestElementBase = ComposeboxEmbedderMixin(I18nMixinLit(CrLitElement));

interface TestComposeboxMixinElement {
  $: {
    input: ComposeboxInputElement,
    matches: ComposeboxDropdownElement,
    inputWrapper: HTMLElement,
  };
}

class TestComposeboxMixinElement extends TestElementBase {
  static get is() {
    return 'test-composebox-mixin';
  }

  override render() {
    // clang-format off
    return html`
      <div id="inputWrapper" @keydown="${this.onKeydown}">
        <cr-composebox-input id="input"
            .result="${this.result}"
            .input="${this.input}"
            .smartComposeEnabled="${this.smartComposeEnabled}"
            .smartComposeInlineHint="${this.smartComposeInlineHint}"
            .cancelButtonTitle="${this.computeCancelButtonTitle()}"
            @input-input="${this.onInputInput}"
            @input-focusin="${this.onInputFocusin}"
            @cancel-click="${this.onCancelClick}"
            @clear-smart-compose="${this.onClearSmartCompose}">
        </cr-composebox-input>
        <cr-composebox-dropdown id="matches"
            .result="${this.result}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
            @match-focusin="${this.onMatchFocusin}"
            @match-click="${this.onMatchClick}">
        </cr-composebox-dropdown>
        <cr-composebox-file-inputs id="fileInputs"
            @file-change="${this.onFileChange}"
            .disableFileInputs="${this.shouldDisableFileInputs()}">
        </cr-composebox-file-inputs>
        ${this.showFileCarousel ? html`
          <cr-composebox-file-carousel
              id="carousel"
              .files="${this.getFilteredCarouselFiles()}"
              @delete-file="${this.onDeleteFile}">
          </cr-composebox-file-carousel>
        ` : ''}
      </div>
    `;
    // clang-format on
  }

  override getInputElement(): ComposeboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  private activeElement_: Element|null = null;
  setActiveElement(elem: Element|null) {
    this.activeElement_ = elem;
  }

  override getActiveElement(): Element|null {
    return this.activeElement_ ?? this.shadowRoot.activeElement;
  }

  override getPageHandler() {
    return ComposeboxProxyImpl.getInstance().handler;
  }

  override getSearchboxCallbackRouter() {
    return ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
  }

  override getSearchboxHandler() {
    return ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override getContextEntrypointElement() {
    return null;
  }
}

customElements.define(
    TestComposeboxMixinElement.is, TestComposeboxMixinElement);

function simulateUserTextInput(
    inputElement: ComposeboxInputElement, value: string): Promise<void> {
  inputElement.input = value;
  inputElement.fire('input-input');
  return microtasksFinished();
}

suite('ComposeboxMixinTest', () => {
  let element: TestComposeboxMixinElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let searchboxHandler: SearchboxPageHandlerRemote&
      TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let metrics: MetricsTracker;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    metrics = fakeMetricsPrivate();
    const callbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            callbackRouter)));
    handler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));

    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setPromiseResolveFor('getInputState', {
      state: new MockInputState(),
    });

    element = document.createElement('test-composebox-mixin') as
        TestComposeboxMixinElement;
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test(
      'refreshTabSuggestions() dedupes restored and current tabs', async () => {
        const tab1 = {
          tabId: 0,
          title: 'Tab 1',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Restored = {
          tabId: 0,
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Recent = {
          tabId: 2,
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab3 = {
          tabId: 3,
          title: 'Tab 3',
          url: 'about:blank?3',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        // Mock searchboxHandler.getRecentTabs to return tab2Recent and tab3.
        searchboxHandler.setResultFor(
            'getRecentTabs', Promise.resolve({tabs: [tab2Recent, tab3]}));

        // Set aimThreadRestoredTabs to contain tab1 and tab2Restored.
        element.aimThreadRestoredTabs = [tab1, tab2Restored];

        await element.refreshTabSuggestions();

        // Expected tabSuggestions: [tab1, tab2Restored, tab3]
        // (tab2Recent from recent tabs should be filtered out because its URL
        // matches tab2Restored)
        assertEquals(3, element.tabSuggestions.length);
        assertEquals(0, element.tabSuggestions[0]!.tabId);
        assertEquals('about:blank?1', element.tabSuggestions[0]!.url);
        assertEquals(0, element.tabSuggestions[1]!.tabId);
        assertEquals('about:blank?2', element.tabSuggestions[1]!.url);
        assertEquals(3, element.tabSuggestions[2]!.tabId);
        assertEquals('about:blank?3', element.tabSuggestions[2]!.url);
      });

  test('submitCleanup() clears active tab selections', async () => {
    const tokenTab = 'test-token-tab' as unknown as UnguessableToken;
    const selectedTabId = 100;
    const mockTabFile = new ComposeboxFile(
        tokenTab, 'Selected Tab', 'tab', InputType.kBrowserTab, {
          isDeletable: true,
          tabId: selectedTabId,
          url: 'about:blank',
        });

    // Add the selected tab to the active files and added tabs maps.
    element.files = new Map([[tokenTab, mockTabFile]]);
    element.addedTabsIds = new Map([[selectedTabId, tokenTab]]);

    await microtasksFinished();

    element.submitCleanup();

    // Verify: The selected Tab 100 must be completely removed from the
    // current active selection.
    assertFalse(element.addedTabsIds.has(selectedTabId));
    assertFalse(element.files.has(tokenTab));
  });

  test(
      'cacheSubmittedTabs appends submitted tabs in chronological order',
      () => {
        const token1 = 'token1' as unknown as UnguessableToken;
        const tab1 =
            new ComposeboxFile(token1, 'Tab 1', 'tab', InputType.kBrowserTab, {
              isDeletable: true,
              tabId: 1,
              url: 'about:blank?1',
            });
        const token2 = 'token2' as unknown as UnguessableToken;
        const tab2 =
            new ComposeboxFile(token2, 'Tab 2', 'tab', InputType.kBrowserTab, {
              isDeletable: true,
              tabId: 2,
              url: 'about:blank?2',
            });

        element.files = new Map([[token1, tab1], [token2, tab2]]);
        element.addedTabsIds = new Map([[1, token1], [2, token2]]);
        element.aimThreadRestoredTabs = [
          {
            tabId: 3,
            title: 'Tab 3',
            url: 'about:blank?3',
            showInCurrentTabChip: false,
            showInPreviousTabChip: false,
            lastActive: {internalValue: 0n},
          },
        ];

        element.cacheSubmittedTabs();

        assertEquals(3, element.aimThreadRestoredTabs.length);
        // Newly submitted tabs (tab1, tab2) should be appended chronologically:
        // [tab3, tab1, tab2]
        assertEquals(3, element.aimThreadRestoredTabs[0]!.tabId);
        assertEquals(1, element.aimThreadRestoredTabs[1]!.tabId);
        assertEquals(2, element.aimThreadRestoredTabs[2]!.tabId);
      });

  test('queryAutocomplete passes cursor position', async () => {
    element.input = 'hello';
    await microtasksFinished();

    const inputElement = element.getInputElement();
    inputElement.inputElement.value = 'hello';
    inputElement.inputElement.focus();
    inputElement.inputElement.selectionStart = 3;
    inputElement.inputElement.selectionEnd = 3;

    searchboxHandler.resetResolver('queryAutocompleteWithSuggestInventory');
    element.queryAutocomplete(/*clearMatches=*/ false);

    const args = await searchboxHandler.whenCalled(
        'queryAutocompleteWithSuggestInventory');
    assertDeepEquals(args, ['hello', false, 3, SuggestInventory.kDefault]);
  });

  test(
      'queryAutocomplete passes cursor position when input is out of sync',
      async () => {
        element.input = 'hello';
        await microtasksFinished();

        const inputElement = element.getInputElement();
        inputElement.inputElement.value = 'hello';
        inputElement.inputElement.focus();
        inputElement.inputElement.selectionStart = 3;
        inputElement.inputElement.selectionEnd = 3;

        // Simulate a programming update of the input as happens when, e.g., the
        // user closes the composebox. This update won't be immediately
        // reflected in the DOM.
        element.input = 'hello world';

        // Clear the `queryAutocompleteWithSuggestInventory` called for ZPS.
        searchboxHandler.resetResolver('queryAutocompleteWithSuggestInventory');
        element.queryAutocomplete(/*clearMatches=*/ false);

        const args = await searchboxHandler.whenCalled(
            'queryAutocompleteWithSuggestInventory');
        assertDeepEquals(
            args, ['hello world', false, 11, SuggestInventory.kDefault]);
      });

  test(
      'Shift+Enter allows inserting a newline when input is focused and not empty',
      async () => {
        element.input = 'Some text';
        await microtasksFinished();

        const inputElement = element.getInputElement();
        inputElement.inputElement.focus();

        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(inputElement.inputElement);

        element.getWrapperElement().dispatchEvent(event);
        await microtasksFinished();

        assertFalse(event.defaultPrevented);
      });

  test(
      'Enter prevents inserting a newline and attempts to submit query when focus is not in dropdown',
      async () => {
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: false,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(element.getInputElement().inputElement);

        element.getWrapperElement().dispatchEvent(event);
        await microtasksFinished();

        assertTrue(event.defaultPrevented);
      });

  test(
      'Shift+Enter submits dropdown selection when focus is in dropdown',
      async () => {
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(element.getDropdownElement());

        element.getWrapperElement().dispatchEvent(event);
        await microtasksFinished();

        assertTrue(event.defaultPrevented);
      });

  test('autocomplete matches are cleared on submit', async () => {
    element.input = 'Some text';
    await microtasksFinished();

    const event = new KeyboardEvent('keydown', {
      key: 'Enter',
      shiftKey: false,
      bubbles: true,
      cancelable: true,
    });
    element.setActiveElement(element.getInputElement().inputElement);
    element.getWrapperElement().dispatchEvent(event);
    await microtasksFinished();

    const clearResult = await searchboxHandler.whenCalled('stopAutocomplete');
    assertTrue(clearResult);
    assertFalse(element.showDropdown);
    assertEquals(null, element.result);
    assertEquals('', element.lastQueriedInput);
  });

  test('smartComposeInlineHint is sliced on sequential typing', async () => {
    element.smartComposeEnabled = true;
    element.input = 'hello';
    element.smartComposeInlineHint = ' world';
    await microtasksFinished();

    const inputElem = element.getInputElement();
    await simulateUserTextInput(inputElem, 'hello ');

    assertEquals('world', element.smartComposeInlineHint);
    assertEquals('hello ', element.input);

    await simulateUserTextInput(inputElem, 'hello w');

    assertEquals('orld', element.smartComposeInlineHint);
  });

  test('smartComposeInlineHint is cleared on non-matching typing', async () => {
    element.smartComposeEnabled = true;
    element.input = 'hello';
    element.smartComposeInlineHint = ' world';
    await microtasksFinished();

    const inputElem = element.getInputElement();
    await simulateUserTextInput(inputElem, 'hello!');

    assertEquals('', element.smartComposeInlineHint);
  });

  test(
      'filters tabs from carousel when tab chips to coins flag is enabled',
      async () => {
        loadTimeData.overrideValues({
          tabFaviconChipsToCoinsEnabled: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        const freshComposebox =
            document.createElement('test-composebox-mixin') as
            TestComposeboxMixinElement;
        document.body.appendChild(freshComposebox);

        const regularFile = ({
                              name: 'image.png',
                              type: 'image/png',
                            } as Partial<ComposeboxFile>) as ComposeboxFile;
        const tabFile = ({
                          name: 'Google',
                          url: 'http://google.com',
                        } as Partial<ComposeboxFile>) as ComposeboxFile;
        freshComposebox.files = new Map([
          ['uuid-1' as unknown as UnguessableToken, regularFile],
          ['uuid-2' as unknown as UnguessableToken, tabFile],
        ]);

        freshComposebox.requestUpdate();
        await microtasksFinished();

        const filteredFiles = freshComposebox.getFilteredCarouselFiles();
        assertEquals(1, filteredFiles.length);
        assertEquals('image.png', filteredFiles[0]!.name);
      });

  test('does not filter tabs from carousel when flag is disabled', async () => {
    loadTimeData.overrideValues({
      tabFaviconChipsToCoinsEnabled: false,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const freshComposebox = document.createElement('test-composebox-mixin') as
        TestComposeboxMixinElement;
    document.body.appendChild(freshComposebox);

    const regularFile = ({
                          name: 'image.png',
                          type: 'image/png',
                        } as Partial<ComposeboxFile>) as ComposeboxFile;
    const tabFile = ({
                      name: 'Google',
                      url: 'http://google.com',
                    } as Partial<ComposeboxFile>) as ComposeboxFile;
    freshComposebox.files = new Map([
      ['uuid-1' as unknown as UnguessableToken, regularFile],
      ['uuid-2' as unknown as UnguessableToken, tabFile],
    ]);

    freshComposebox.requestUpdate();
    await microtasksFinished();

    const filteredFiles = freshComposebox.getFilteredCarouselFiles();
    assertEquals(2, filteredFiles.length);
  });

  test(
      'onOpenDriveUpload suppresses upload if disclaimer not accepted',
      async () => {
        searchboxHandler.setResultFor(
            'getDriveDisclaimerStatus',
            Promise.resolve({status: DriveDisclaimerStatus.kNotAccepted}));

        await element.onOpenDriveUpload();

        assertEquals(
            1, searchboxHandler.getCallCount('getDriveDisclaimerStatus'));
        assertEquals(0, searchboxHandler.getCallCount('onDriveUploadClicked'));
      });

  test('onOpenDriveUpload triggers upload if disclaimer accepted', async () => {
    searchboxHandler.setResultFor(
        'getDriveDisclaimerStatus',
        Promise.resolve({status: DriveDisclaimerStatus.kAccepted}));
    searchboxHandler.setResultFor(
        'onDriveUploadClicked',
        Promise.resolve({response: {files: [], error: null}}));

    await element.onOpenDriveUpload();

    assertEquals(1, searchboxHandler.getCallCount('getDriveDisclaimerStatus'));
    assertEquals(1, searchboxHandler.getCallCount('onDriveUploadClicked'));
  });

  test('addDriveUploads adds files to composebox', async () => {
    const token = {high: 1n, low: 1n} as unknown as UnguessableToken;
    element.addDriveUploads([{
      token,
      mimeType: 'image/png',
      fileName: 'file.png',
      thumbnailUrl: 'thumb',
      iconUrl: 'icon',
    }]);

    await microtasksFinished();
    assertTrue(element.files.has(token));
    const file = element.files.get(token)!;
    assertEquals('file.png', file.name);
    assertEquals('image/png', file.type);
    assertFalse(element.showDropdown);
  });

  test('addDriveUploads handles max files exceeded error', async () => {
    element.addDriveUploads([], DriveUploadError.kMaxFilesExceeded);
    await microtasksFinished();
    assertEquals(element.i18n('maxFilesReachedError'), element.errorMessage);
  });

  test('addDriveUploads handles size limit exceeded error', async () => {
    element.addDriveUploads([], DriveUploadError.kSizeLimitExceeded);
    await microtasksFinished();
    assertEquals(
        element.i18n(
            'composeboxFileUploadInvalidTooLarge',
            Math.floor(element.maxFileSize / (1024 * 1024))),
        element.errorMessage);
  });

  test('updateState calls addDriveUploads for drive uploads', async () => {
    const token = {high: 2n, low: 2n} as unknown as UnguessableToken;
    element.state = {
      text: 'hello',
      files: [{
        token,
        mimeType: 'image/png',
        fileName: 'file.png',
        thumbnailUrl: 'thumb',
        iconUrl: 'icon',
      }],
      mode: 0,
      model: 0,
    };
    await microtasksFinished();
    assertTrue(element.files.has(token));
    assertEquals('hello', element.input);
  });

  test('onInputStateChanged updates inputState property', async () => {
    const inputState = {
      allowedModels: [],
      allowedTools: [],
      allowedInputTypes: [],
      activeModel: ModelMode.kGeminiRegular,
      activeTool: ToolMode.kUnspecified,
      disabledModels: [],
      disabledTools: [],
      disabledInputTypes: [],
      inputTypeConfigs: [],
      toolConfigs: [],
      modelConfigs: [],
      toolsSectionConfig: null,
      modelSectionConfig: null,
      hintText: 'Hint',
      maxInputsByType: {},
      maxTotalInputs: 5,
      isCanvasQuerySubmitted: false,
    } as unknown as InputState;
    element.onInputStateChanged(inputState);
    await microtasksFinished();
    assertDeepEquals(element.inputState, inputState);
  });

  test('NotifySessionStarted called on connectedCallback', () => {
    assertEquals(1, searchboxHandler.getCallCount('notifySessionStarted'));
  });

  test('clear button title changes with input text', async () => {
    const cancelIcon =
        element.getInputElement().shadowRoot.querySelector('#cancelIcon')!;
    assertEquals(
        loadTimeData.getString('composeboxCancelButtonTitle'),
        cancelIcon.getAttribute('title'));

    await simulateUserTextInput(element.getInputElement(), 'Test');
    assertEquals(
        loadTimeData.getString('composeboxCancelButtonTitleInput'),
        cancelIcon.getAttribute('title'));
  });

  test(
      'updates state from state property with regular files and modes',
      async () => {
        assertEquals(0, searchboxHandler.getCallCount('setActiveToolMode'));
        assertEquals(0, searchboxHandler.getCallCount('setActiveModelMode'));

        const mockFile =
            new File(['test'], 'test.pdf', {type: 'application/pdf'});
        element.state = {
          text: 'hello world',
          files: [{file: mockFile}],
          mode: ToolMode.kDeepSearch,
          model: ModelMode.kGeminiRegular,
        };
        await searchboxHandler.whenCalled('addFileContext');
        await element.updateComplete;

        assertEquals('hello world', element.input);
        assertEquals(1, element.files.size);
        assertEquals(1, searchboxHandler.getCallCount('setActiveToolMode'));
        assertEquals(
            ToolMode.kDeepSearch,
            searchboxHandler.getArgs('setActiveToolMode')[0]);
        assertEquals(1, searchboxHandler.getCallCount('setActiveModelMode'));
        assertEquals(
            ModelMode.kGeminiRegular,
            searchboxHandler.getArgs('setActiveModelMode')[0]);
      });

  test('navigates matches with ArrowDown and ArrowUp', async () => {
    const input = element.getInputElement().inputElement;
    const matchesElement = element.getDropdownElement();

    element.result = {input: '', matches: []} as unknown as AutocompleteResult;
    await microtasksFinished();

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(-1, matchesElement.selectedMatchIndex);

    const matches = [
      {fillIntoEdit: 'test1'} as AutocompleteMatch,
      {fillIntoEdit: 'test2'} as AutocompleteMatch,
    ];
    element.result = {input: 'test', matches} as AutocompleteResult;
    await microtasksFinished();

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

    input.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      ctrlKey: true,
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);

    element.dropdownNeeded = false;
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);
  });

  test('selects first or last match with PageUp and PageDown', async () => {
    const input = element.getInputElement().inputElement;
    const matchesElement = element.getDropdownElement();

    const matches = [
      {fillIntoEdit: 'test1'} as AutocompleteMatch,
      {fillIntoEdit: 'test2'} as AutocompleteMatch,
      {fillIntoEdit: 'test3'} as AutocompleteMatch,
    ];
    element.result = {input: 'test', matches} as AutocompleteResult;
    await microtasksFinished();

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'PageDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(2, matchesElement.selectedMatchIndex);

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'PageUp', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);

    input.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'PageDown',
      altKey: true,
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);
  });

  test(
      'PageDown and PageUp are ignored when no matches are available',
      async () => {
        const input = element.getInputElement().inputElement;
        const matchesElement = element.getDropdownElement();

        input.dispatchEvent(new KeyboardEvent(
            'keydown', {key: 'PageDown', bubbles: true, composed: true}));
        await element.updateComplete;
        assertEquals(-1, matchesElement.selectedMatchIndex);
      });

  test('Tab behavior when focus is in input', async () => {
    element.smartComposeEnabled = true;
    const inputElem = element.getInputElement();
    const input = inputElem.inputElement;
    const matchesElement = element.getDropdownElement();

    const matches = [{fillIntoEdit: 'match1'} as AutocompleteMatch];
    element.result = {input: 'tes', matches} as AutocompleteResult;
    await microtasksFinished();

    matchesElement.selectNext();
    assertEquals(0, matchesElement.selectedMatchIndex);
    input.focus();

    input.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'Tab', shiftKey: true, bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(-1, matchesElement.selectedMatchIndex);

    await simulateUserTextInput(inputElem, 'tes');
    element.smartComposeInlineHint = 't';
    await element.updateComplete;

    const tabEvent = new KeyboardEvent(
        'keydown',
        {key: 'Tab', bubbles: true, cancelable: true, composed: true});
    input.dispatchEvent(tabEvent);
    await microtasksFinished();

    assertEquals('test', input.value);
    assertTrue(tabEvent.defaultPrevented);
  });

  test('Tab on last dropdown match unselects active match', async () => {
    const matchesElement = element.getDropdownElement();
    const matches = [
      {fillIntoEdit: 'match1', supportsDeletion: false} as AutocompleteMatch,
      {fillIntoEdit: 'match2', supportsDeletion: false} as AutocompleteMatch,
    ];
    element.result = {input: 'm', matches} as AutocompleteResult;
    await microtasksFinished();

    matchesElement.selectNext();
    matchesElement.selectNext();
    assertEquals(1, matchesElement.selectedMatchIndex);

    await matchesElement.updateComplete;
    element.setActiveElement(matchesElement);

    const tabEvent = new KeyboardEvent(
        'keydown',
        {key: 'Tab', bubbles: true, cancelable: true, composed: true});
    matchesElement.dispatchEvent(tabEvent);
    await element.updateComplete;

    assertEquals(-1, matchesElement.selectedMatchIndex);
    assertFalse(tabEvent.defaultPrevented);
  });

  test('Tab in dropdown is ignored when key modifiers are active', async () => {
    const matchesElement = element.getDropdownElement();
    const matches = [
      {fillIntoEdit: 'match1', supportsDeletion: false} as AutocompleteMatch,
      {fillIntoEdit: 'match2', supportsDeletion: false} as AutocompleteMatch,
    ];
    element.result = {input: 'm', matches} as AutocompleteResult;
    await element.updateComplete;

    matchesElement.selectNext();
    matchesElement.selectNext();
    await matchesElement.updateComplete;
    await element.updateComplete;
    element.setActiveElement(matchesElement);
    const tabEventCtrl = new KeyboardEvent('keydown', {
      key: 'Tab',
      ctrlKey: true,
      bubbles: true,
      cancelable: true,
    });
    matchesElement.dispatchEvent(tabEventCtrl);
    await element.updateComplete;
    assertEquals(1, matchesElement.selectedMatchIndex);
  });

  test('Tab in dropdown is ignored when no matches are available', async () => {
    const matchesElement = element.getDropdownElement();

    const tabEventNoMatch = new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      cancelable: true,
    });
    matchesElement.dispatchEvent(tabEventNoMatch);
    await element.updateComplete;
    assertEquals(-1, matchesElement.selectedMatchIndex);
  });

  test('Smart Compose hint is hidden during backspacing', async () => {
    element.smartComposeEnabled = true;
    const inputElem = element.getInputElement();
    const input = inputElem.inputElement;

    await simulateUserTextInput(inputElem, 'tes');
    element.smartComposeInlineHint = 't';
    await element.updateComplete;

    assertTrue(!!inputElem.shadowRoot.querySelector('#smartCompose'));

    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Backspace'}));
    await microtasksFinished();

    assertFalse(!!inputElem.shadowRoot.querySelector('#smartCompose'));
  });

  test('Smart Compose hint is hidden when cursor is not at end', async () => {
    element.smartComposeEnabled = true;
    const inputElem = element.getInputElement();
    const input = inputElem.inputElement;

    await simulateUserTextInput(inputElem, 'test');
    element.smartComposeInlineHint = 'a';
    await element.updateComplete;

    assertTrue(!!inputElem.shadowRoot.querySelector('#smartCompose'));

    input.selectionStart = 2;
    input.selectionEnd = 2;
    inputElem.requestUpdate();
    await microtasksFinished();

    assertFalse(!!inputElem.shadowRoot.querySelector('#smartCompose'));
  });

  test(
      'Smart Compose hint is hidden when it wraps in the middle of a word',
      async () => {
        const inputElement = element.getInputElement();
        const input = inputElement.inputElement as HTMLTextAreaElement;

        const originalMeasureText =
            CanvasRenderingContext2D.prototype.measureText;
        try {
          CanvasRenderingContext2D.prototype.measureText = function(
              text: string) {
            if (text.includes('wrap')) {
              return {width: 150} as TextMetrics;
            }
            return {width: 50} as TextMetrics;
          };
          Object.defineProperty(
              input, 'clientWidth', {configurable: true, get: () => 100});

          element.smartComposeEnabled = true;
          await simulateUserTextInput(inputElement, 'tes.');
          element.smartComposeInlineHint = 'wrap';
          await element.updateComplete;

          assertFalse(!!inputElement.shadowRoot.querySelector('#smartCompose'));
        } finally {
          CanvasRenderingContext2D.prototype.measureText = originalMeasureText;
        }
      });

  test(
      'Smart Compose hint is NOT hidden when only full hint wraps but first word fits',
      async () => {
        const inputElement = element.getInputElement();
        const input = inputElement.inputElement as HTMLTextAreaElement;

        const originalMeasureText =
            CanvasRenderingContext2D.prototype.measureText;
        try {
          CanvasRenderingContext2D.prototype.measureText = function(
              text: string) {
            if (text.includes('wraps')) {
              return {width: 150} as TextMetrics;
            }
            return {width: 50} as TextMetrics;
          };
          Object.defineProperty(
              input, 'clientWidth', {configurable: true, get: () => 100});

          element.smartComposeEnabled = true;
          await simulateUserTextInput(inputElement, 'tes.');
          element.smartComposeInlineHint = 'fits wraps';
          await element.updateComplete;

          assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));
        } finally {
          CanvasRenderingContext2D.prototype.measureText = originalMeasureText;
        }
      });

  test(
      'Tab key does not accept Smart Compose when hidden by wrapping',
      async () => {
        const inputElement = element.getInputElement();
        const input = inputElement.inputElement as HTMLTextAreaElement;

        const originalMeasureText =
            CanvasRenderingContext2D.prototype.measureText;
        try {
          CanvasRenderingContext2D.prototype.measureText = function(
              text: string) {
            if (text.includes('wrap')) {
              return {width: 150} as TextMetrics;
            }
            return {width: 50} as TextMetrics;
          };
          Object.defineProperty(
              input, 'clientWidth', {configurable: true, get: () => 100});

          element.smartComposeEnabled = true;
          await simulateUserTextInput(inputElement, 'tes.');
          element.smartComposeInlineHint = 'wrap';
          await element.updateComplete;

          element.setActiveElement(input);
          const tabEvent = new KeyboardEvent('keydown', {
            key: 'Tab',
            bubbles: true,
            cancelable: true,
          });
          element.getWrapperElement().dispatchEvent(tabEvent);
          await element.updateComplete;

          assertEquals('tes.', element.input);
        } finally {
          CanvasRenderingContext2D.prototype.measureText = originalMeasureText;
        }
      });

  test('sets and deletes visual selection thumbnail', async () => {
    assertFalse(element.showFileCarousel);

    const thumbnailUrl = 'data:image/png;base64,sometestdata';
    const testToken =
        '12345678901234567890123456789012' as unknown as UnguessableToken;
    searchboxCallbackRouterRemote.addFileContext(testToken, {
      fileName: 'Visual Selection',
      mimeType: 'image/png',
      imageDataUrl: thumbnailUrl,
      isDeletable: true,
      selectionTime: new Date(),
    } as SelectedFileInfo);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await element.updateComplete;

    assertTrue(element.showFileCarousel);
    const fileCarousel =
        element.shadowRoot.querySelector<ComposeboxFileCarouselElement>(
            '#carousel')!;
    assertTrue(!!fileCarousel);
    await fileCarousel.updateComplete;

    assertEquals(1, fileCarousel.files.length);
    assertEquals(testToken, fileCarousel.files[0]!.uuid);
    assertEquals(thumbnailUrl, fileCarousel.files[0]!.dataUrl);
    assertTrue(fileCarousel.files[0]!.isDeletable);

    const fileThumbnail =
        fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
    assertTrue(!!fileThumbnail);
    const removeImgButton =
        fileThumbnail.shadowRoot.querySelector<HTMLElement>('#removeImgButton');
    assertTrue(!!removeImgButton);
    removeImgButton.click();
    await element.updateComplete;

    assertEquals(1, searchboxHandler.getCallCount('deleteContext'));
    const [idArg, fromChip] = searchboxHandler.getArgs('deleteContext')[0]!;
    assertEquals(testToken, idArg);
    assertFalse(fromChip);
    assertFalse(element.showFileCarousel);
    assertFalse(!!element.shadowRoot.querySelector('#carousel'));
  });

  test('setVisualSelectionThumbnail not deletable', async () => {
    const thumbnailUrl = 'data:image/png;base64,sometestdata';
    const testToken =
        '12345678901234567890123456789013' as unknown as UnguessableToken;
    searchboxCallbackRouterRemote.addFileContext(testToken, {
      fileName: 'Visual Selection',
      mimeType: 'image/png',
      imageDataUrl: thumbnailUrl,
      isDeletable: false,
      selectionTime: new Date(),
    } as SelectedFileInfo);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await element.updateComplete;

    assertTrue(element.showFileCarousel);
    const fileCarousel =
        element.shadowRoot.querySelector<ComposeboxFileCarouselElement>(
            '#carousel')!;
    assertTrue(!!fileCarousel);
    await fileCarousel.updateComplete;

    assertEquals(1, fileCarousel.files.length);
    assertFalse(fileCarousel.files[0]!.isDeletable);

    const fileThumbnail =
        fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
    assertTrue(!!fileThumbnail);
    const removeButton =
        fileThumbnail.shadowRoot.querySelector<HTMLElement>('#removeImgButton');
    assertEquals(null, removeButton);
  });

  test(
      'clears suggestInventory when properties change and has content',
      async () => {
        element.suggestInventory = SuggestInventory.kTravel;
        assertEquals(SuggestInventory.kTravel, element.suggestInventory);

        const emptyInputState =
            new MockInputState({activeTool: ToolMode.kUnspecified});
        element.onInputStateChanged(emptyInputState);
        await element.updateComplete;
        assertEquals(SuggestInventory.kTravel, element.suggestInventory);

        const toolInputState =
            new MockInputState({activeTool: ToolMode.kDeepSearch});
        element.onInputStateChanged(toolInputState);
        await element.updateComplete;
        assertEquals(null, element.suggestInventory);
      });

  test('delete tool chip', async () => {
    element.composeboxSource = 'TestEmbedder';
    const inputState = new MockInputState({activeTool: ToolMode.kDeepSearch});
    element.onInputStateChanged(inputState);
    await element.updateComplete;

    element.handleToolClick(ToolMode.kDeepSearch);
    await element.updateComplete;

    assertEquals(1, searchboxHandler.getCallCount('setActiveToolMode'));
    assertEquals(
        ToolMode.kUnspecified,
        searchboxHandler.getArgs('setActiveToolMode')[0]);

    const metricName =
        'ContextualSearch.UserAction.InputStateDeletion.TestEmbedder';
    assertEquals(
        1,
        metrics.count(metricName, ContextualSearchInputStateDeletionType.TOOL));
  });

  test('setDefaultModel uses activeModel from backend', async () => {
    const inputState = new MockInputState({
      allowedModels: [ModelMode.kGeminiRegular, ModelMode.kGeminiPro],
      activeModel: ModelMode.kGeminiPro,
      modelConfigs: [
        {
          model: ModelMode.kGeminiRegular,
          aimUrlParams: [],
          menuLabel: 'Regular',
          hintText: 'Hint Regular',
          menuTooltip: '',
        },
        {
          model: ModelMode.kGeminiPro,
          aimUrlParams: [{paramKey: 'xyz', paramValue: '1'}],
          menuLabel: 'Pro',
          hintText: 'Hint Pro',
          menuTooltip: '',
        },
      ],
      modelSectionConfig: null,
    });
    element.onInputStateChanged(inputState);
    await element.updateComplete;

    element.setDefaultModel();

    assertEquals(1, searchboxHandler.getCallCount('setActiveModelMode'));
    assertEquals(
        ModelMode.kGeminiPro,
        searchboxHandler.getArgs('setActiveModelMode')[0]);
  });

  test('empty input computes canSubmitFilesAndInput as false', async () => {
    await simulateUserTextInput(element.getInputElement(), '');
    assertFalse(element.canSubmitFilesAndInput);
  });

  test(
      'whitespace input computes canSubmitFilesAndInput as false', async () => {
        await simulateUserTextInput(element.getInputElement(), ' ');
        assertFalse(element.canSubmitFilesAndInput);
      });

  test(
      'submitQuery is a no-op when canSubmitFilesAndInput is false',
      async () => {
        assertEquals(0, searchboxHandler.getCallCount('submitQuery'));
        assertEquals(0, searchboxHandler.getCallCount('openAutocompleteMatch'));

        await simulateUserTextInput(element.getInputElement(), '');
        assertFalse(element.canSubmitFilesAndInput);

        element.submitQuery();
        assertEquals(0, searchboxHandler.getCallCount('submitQuery'));
        assertEquals(0, searchboxHandler.getCallCount('openAutocompleteMatch'));
      });

  test('metrics are recorded for ToolMode clicks', () => {
    element.composeboxSource = 'TestEmbedder';

    const metricName =
        'TestEmbedder.AimEntrypoint.AimPopup.ContextualElement.Clicked';
    element.onToolClick(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kDeepSearch},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.DEEP_RESEARCH));
    assertEquals(1, metrics.count(`${metricName}.DeepResearch`, 0));

    element.onToolClick(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kCanvas},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.CANVAS));
    assertEquals(1, metrics.count(`${metricName}.Canvas`, 0));

    element.onToolClick(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kImageGen},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.IMAGE_GEN));
    assertEquals(1, metrics.count(`${metricName}.ImageGen`, 0));
  });

  test('metrics are recorded for ModelMode clicks', () => {
    element.composeboxSource = 'TestEmbedder';

    const metricName =
        'TestEmbedder.AimEntrypoint.AimPopup.ContextualElement.Clicked';
    element.onModelClick(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiProAutoroute},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.AUTO_MODEL));
    assertEquals(1, metrics.count(`${metricName}.AutoModel`, 0));

    element.onModelClick(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiPro},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.THINKING_MODEL));
    assertEquals(1, metrics.count(`${metricName}.ThinkingModel`, 0));

    element.onModelClick(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiRegular},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.REGULAR_MODEL));
    assertEquals(1, metrics.count(`${metricName}.RegularModel`, 0));

    element.onModelClick(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiProNoGenUi},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.PRO_NO_GEN_UI_MODEL));
    assertEquals(1, metrics.count(`${metricName}.ProNoGenUiModel`, 0));
  });

  test('metrics are recorded for file uploads', () => {
    element.composeboxSource = 'TestEmbedder';

    const metricName =
        'TestEmbedder.AimEntrypoint.AimPopup.ContextualElement.Clicked';

    element.onOpenImageUpload();
    assertEquals(1, metrics.count(metricName, ContextType.IMAGE));
    assertEquals(1, metrics.count(`${metricName}.Image`, 0));

    element.onOpenFileUpload();
    assertEquals(1, metrics.count(metricName, ContextType.FILE));
    assertEquals(1, metrics.count(`${metricName}.File`, 0));
  });

  test('metrics are recorded for tab additions', () => {
    element.composeboxSource = 'TestEmbedder';

    const metricName =
        'TestEmbedder.AimEntrypoint.AimPopup.ContextualElement.Clicked';

    element.onAddTabContext(new CustomEvent('add-tab-context', {
      detail: {
        id: 1,
        title: 'Title',
        url: 'http://test.com',
        delayUpload: false,
        origin: TabUploadOrigin.OTHER,
      },
    }));
    assertEquals(1, metrics.count(metricName, ContextType.TAB));
    assertEquals(1, metrics.count(`${metricName}.Tab`, 0));
  });

  test('session abandoned on cancel button click', async () => {
    element.suggestInventory = SuggestInventory.kTravel;
    await element.updateComplete;

    const whenCloseComposebox =
        eventToPromise<CustomEvent<{composeboxText: string}>>(
            'close-composebox', element);
    const cancelIcon =
        element.getInputElement().shadowRoot.querySelector<HTMLElement>(
            '#cancelIcon')!;
    cancelIcon.click();
    const event = await whenCloseComposebox;
    assertEquals('', event.detail.composeboxText);
    assertEquals(1, searchboxHandler.getCallCount('clearFiles'));
    assertEquals(null, element.suggestInventory);
  });

  test('session abandoned on esc click', async () => {
    element.closeOnEscape = true;
    await simulateUserTextInput(element.getInputElement(), 'test');
    element.suggestInventory = SuggestInventory.kTravel;
    await element.updateComplete;

    const whenCloseComposebox =
        eventToPromise<CustomEvent<{composeboxText: string}>>(
            'close-composebox', element);
    element.getWrapperElement().dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Escape'}));
    await element.updateComplete;

    const event = await whenCloseComposebox;
    assertEquals('test', event.detail.composeboxText);
    assertEquals(1, searchboxHandler.getCallCount('clearFiles'));
    assertEquals(null, element.suggestInventory);
  });

  test(
      'esc clears input instead of closing when closeOnEscape is false and has content',
      async () => {
        element.closeOnEscape = false;
        await simulateUserTextInput(element.getInputElement(), 'test');
        await element.updateComplete;

        const closePromise = eventToPromise('close-composebox', element);
        let closed = false;
        closePromise.then(() => closed = true);

        element.getWrapperElement().dispatchEvent(
            new KeyboardEvent('keydown', {key: 'Escape'}));
        await element.updateComplete;

        assertFalse(closed);
        assertEquals('', element.getInputElement().input);
        assertEquals(1, searchboxHandler.getCallCount('clearFiles'));
      });
});
