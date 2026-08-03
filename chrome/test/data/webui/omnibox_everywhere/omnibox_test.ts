// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';

import {ComposeboxProxyImpl, SearchboxBrowserProxy} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import type {OmniboxEverywhereAppElement, OmniboxEverywhereComposeboxElement, OmniboxEverywhereOmniboxElement} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import {TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxState} from 'chrome://resources/cr_components/composebox/common.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('OmniboxEverywhereOmniboxTest', () => {
  let omnibox: OmniboxEverywhereOmniboxElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      ntpRealboxNextEnabled: true,
      searchboxVoiceSearch: true,
      composeboxContextDragAndDropEnabled: true,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    omnibox = document.createElement('omnibox-everywhere-omnibox');
    document.body.appendChild(omnibox);
    await microtasksFinished();
  });

  test('onAddTabContext_ opens composebox with tab upload', () => {
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
});

suite('OmniboxEverywhereComposeboxTest', () => {
  let composebox: OmniboxEverywhereComposeboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let mockPageHandler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      composeboxContextDragAndDropEnabled: true,
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

  test('onAddTabContext adds tab to composebox files', async () => {
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
    assertEquals(1, composebox.files.size);
    const file = Array.from(composebox.files.values())[0]!;
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
      ntpRealboxNextEnabled: true,
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
      'voice search final result populates searchbox input and closes dialog',
      async () => {
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
        assertEquals(
            'test query from speech', searchbox.$.input.inputElement.value);
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
});
