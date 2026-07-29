// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://notebooks-internals/app.js';

import type {NotebooksInternalsAppElement} from 'chrome://notebooks-internals/app.js';
import {browserProxyFactory} from 'chrome://notebooks-internals/notebooks_internals.mojom-webui.js';
import type {PageRemote} from 'chrome://notebooks-internals/notebooks_internals.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TEST_NOTEBOOK_HOME_URL, TestNotebooksInternalsPageHandler} from './test_notebooks_internals_page_handler.js';

suite('NotebooksInternalsTest', () => {
  let app: NotebooksInternalsAppElement;
  let pageHandler: TestNotebooksInternalsPageHandler;
  let pageRemote: PageRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    pageHandler = new TestNotebooksInternalsPageHandler();
    const {instance, remote} = browserProxyFactory.createForTest(pageHandler);
    browserProxyFactory.setInstance(instance);
    pageRemote = remote;
  });

  async function createApp(): Promise<NotebooksInternalsAppElement> {
    app = document.createElement('notebooks-internals-app');
    document.body.appendChild(app);
    // connectedCallback() fetches feature flags and eligibility state, so we
    // must wait another render cycle for the data to be populated.
    await microtasksFinished();
    return app;
  }

  test('has expected page title', () => {
    assertEquals('notebooks internals', document.title.toLocaleLowerCase());
  });

  test('fetches data from browser proxy on initialization', async () => {
    await createApp();
    await pageHandler.whenCalled('getFeatureFlagState');
    await pageHandler.whenCalled('getProfileEligibility');
    assertTrue(!!app);
  });

  test(
      'displays eligible user, enabled feature flag, and custom home url',
      async () => {
        pageHandler.setFeatureFlagState({
          notebooksFeatureEnabled: true,
          notebookHomeUrl: TEST_NOTEBOOK_HOME_URL,
        });
        pageHandler.setProfileEligibility({
          userEligible: true,
        });

        await createApp();

        const eligibilityCells = app.$.eligibilityTable.querySelectorAll('td');
        assertEquals(4, eligibilityCells.length);

        assertEquals(
            'Is user eligible?', eligibilityCells[0]!.textContent.trim());
        assertEquals('Yes', eligibilityCells[1]!.textContent.trim());

        assertEquals(
            'notebooks feature flag enabled?',
            eligibilityCells[2]!.textContent.trim());
        assertEquals('Yes', eligibilityCells[3]!.textContent.trim());

        const configCells = app.$.configTable.querySelectorAll('td');
        assertEquals(2, configCells.length);

        assertEquals('Notebook home URL', configCells[0]!.textContent.trim());
        assertEquals(
            TEST_NOTEBOOK_HOME_URL, configCells[1]!.textContent.trim());
      });

  test(
      'displays ineligible user, disabled feature flag, and default url',
      async () => {
        pageHandler.setFeatureFlagState({
          notebooksFeatureEnabled: false,
          notebookHomeUrl: '',
        });
        pageHandler.setProfileEligibility({
          userEligible: false,
        });

        await createApp();

        const eligibilityCells = app.$.eligibilityTable.querySelectorAll('td');
        assertEquals(4, eligibilityCells.length);

        assertEquals(
            'Is user eligible?', eligibilityCells[0]!.textContent.trim());
        assertEquals('No', eligibilityCells[1]!.textContent.trim());

        assertEquals(
            'notebooks feature flag enabled?',
            eligibilityCells[2]!.textContent.trim());
        assertEquals('No', eligibilityCells[3]!.textContent.trim());

        const configCells = app.$.configTable.querySelectorAll('td');
        assertEquals(2, configCells.length);

        assertEquals('Notebook home URL', configCells[0]!.textContent.trim());
        assertEquals('(default)', configCells[1]!.textContent.trim());
      });

  test(
      'updates profile eligibility when event is fired from browser',
      async () => {
        pageHandler.setProfileEligibility({userEligible: false});
        await createApp();

        let eligibilityCells = app.$.eligibilityTable.querySelectorAll('td');
        assertEquals('No', eligibilityCells[1]!.textContent.trim());

        pageRemote.onProfileEligibilityChanged({userEligible: true});
        await microtasksFinished();

        eligibilityCells = app.$.eligibilityTable.querySelectorAll('td');
        assertEquals('Yes', eligibilityCells[1]!.textContent.trim());
      });
});
