// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/internals/app.js';

import {browserProxyFactory} from 'chrome://contextual-tasks/contextual_tasks_internals.mojom-webui.js';
import type {ContextualTasksInternalsPageHandlerInterface, EligibilityState, GetRelevantContextRequest} from 'chrome://contextual-tasks/contextual_tasks_internals.mojom-webui.js';
import type {ContextualTasksInternalsAppElement} from 'chrome://contextual-tasks/internals/app.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestContextualTasksInternalsPageHandler extends TestBrowserProxy
    implements ContextualTasksInternalsPageHandlerInterface {
  private host_: string = '';

  constructor() {
    super([
      'getRelevantContext',
      'setForcedEmbeddedPageHost',
      'getForcedEmbeddedPageHost',
      'getEligibilityState',
      'getSessionAndUserDataState',
    ]);
  }

  getRelevantContext(request: GetRelevantContextRequest) {
    this.methodCalled('getRelevantContext', request);
    return Promise.resolve({response: {relevantTabs: []}});
  }

  setForcedEmbeddedPageHost(host: string) {
    this.methodCalled('setForcedEmbeddedPageHost', host);
    this.host_ = host;
  }

  getForcedEmbeddedPageHost() {
    this.methodCalled('getForcedEmbeddedPageHost');
    return Promise.resolve({host: this.host_});
  }

  getEligibilityState() {
    this.methodCalled('getEligibilityState');
    const state: EligibilityState = {
      isEligible: true,
      isContextualTasksEnabled: true,
      isSignedIn: true,
      primaryAccountInCookieJar: true,
      isDefaultSearchEngineGoogle: true,
      isAimEligible: true,
      isCobrowseEligible: true,
      isContextSharingEnabled: true,
      isAimAllowedByPolicy: true,
      isRearchitectureEnabled: true,
      isContextualTasksRearchitectureEnabled: true,
      isSidePanelRearchitectureEnabled: true,
      isApiContextualTasksPrivateEnabled: true,
      isFuseboxEligible: true,
      isAimTabsPickerEligible: true,
      isRearchitectedSidePanelEligible: true,
    };
    return Promise.resolve({state});
  }

  getSessionAndUserDataState() {
    this.methodCalled('getSessionAndUserDataState');
    return Promise.resolve({webContentsStates: []});
  }
}

async function setInputValue(input: CrInputElement, value: string) {
  input.value = value;
  await microtasksFinished();
}

suite('ContextualTasksInternalsAppTest', () => {
  let app: ContextualTasksInternalsAppElement;
  let handler: TestContextualTasksInternalsPageHandler;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = new TestContextualTasksInternalsPageHandler();
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);

    app = document.createElement('contextual-tasks-internals-app');
    document.body.appendChild(app);
    await handler.whenCalled('getForcedEmbeddedPageHost');
    await microtasksFinished();
  });

  test(
      'adds, edits, removes forced params and updates effective URL',
      async () => {
        assertEquals(
            'https://www.google.com/search?...',
            app.$.effectiveUrlDisplay.textContent?.trim());

        // Add key/value pairs via inputs and button click.
        await setInputValue(app.$.paramKeyInput, 'gl');
        await setInputValue(app.$.paramValueInput, 'us');
        app.$.addForcedParamButton.click();
        await microtasksFinished();
        assertEquals(
            'https://www.google.com/search?...&gl=us',
            app.$.effectiveUrlDisplay.textContent?.trim());

        await setInputValue(app.$.paramKeyInput, 'deb');
        await setInputValue(app.$.paramValueInput, '0');
        app.$.addForcedParamButton.click();
        await microtasksFinished();
        assertEquals(
            'https://www.google.com/search?...&deb=0&gl=us',
            app.$.effectiveUrlDisplay.textContent?.trim());

        // Click chip to populate inputs for editing.
        const editButtons = app.shadowRoot.querySelectorAll<HTMLButtonElement>(
            '.param-chip-edit');
        assertEquals(2, editButtons.length);
        editButtons[0]!.click();
        await microtasksFinished();
        assertEquals('deb', app.$.paramKeyInput.value);
        assertEquals('0', app.$.paramValueInput.value);

        // Remove a single chip.
        const removeButtons =
            app.shadowRoot.querySelectorAll<HTMLButtonElement>(
                '.param-chip-remove');
        removeButtons[0]!.click();
        await microtasksFinished();
        assertEquals(
            'https://www.google.com/search?...&gl=us',
            app.$.effectiveUrlDisplay.textContent?.trim());
      });

  test(
      'unified card updates host, preview, and resets all overrides',
      async () => {
        await setInputValue(app.$.forcedHostInput, 'gws-prod.corp.google.com');
        app.$.setForcedHostButton.click();
        const hostArg = await handler.whenCalled('setForcedEmbeddedPageHost');
        await microtasksFinished();
        assertEquals('https://gws-prod.corp.google.com/', hostArg);
        assertFalse(app.$.clearForcedHostButton.hidden);

        await setInputValue(app.$.paramKeyInput, 'deb');
        await setInputValue(app.$.paramValueInput, '0');
        app.$.addForcedParamButton.click();
        await microtasksFinished();
        assertEquals(
            'https://gws-prod.corp.google.com/search?...&deb=0',
            app.$.effectiveUrlDisplay.textContent?.trim());

        // Reset All clears both host and params.
        handler.resetResolver('setForcedEmbeddedPageHost');
        app.$.resetAllOverridesButton.click();
        const resetHost = await handler.whenCalled('setForcedEmbeddedPageHost');
        await microtasksFinished();
        assertEquals('', resetHost);
        assertTrue(app.$.clearForcedHostButton.hidden);
        assertEquals(
            'https://www.google.com/search?...',
            app.$.effectiveUrlDisplay.textContent?.trim());
      });
});
