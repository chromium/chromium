// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsOnDeviceAiPageElement, SettingsOnDeviceAiPageEntryElement} from 'chrome://settings/lazy_load.js';
import {OnDeviceAiBrowserProxyImpl, OnDeviceModelStatus} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl, PrefsBrowserProxy, PrefService, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createAsrCapability, createGemmaCapability} from './on_device_ai_page_test_util.js';
import {TestOnDeviceAiBrowserProxy} from './test_on_device_ai_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

const ON_DEVICE_AI_PREF_KEY = 'synthetic.settings.on_device_ai_enabled';

suite('OnDeviceAiPage', function() {
  let testBrowserProxy: TestOnDeviceAiBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let page: SettingsOnDeviceAiPageElement;

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-on-device-ai-page');
    document.body.appendChild(page);
    await Promise.all([
      testBrowserProxy.whenCalled('getOnDeviceAiEnabled'),
      testBrowserProxy.whenCalled('getOnDeviceAiCapabilities'),
    ]);
    await microtasksFinished();
  }

  function getModelEntries(): NodeListOf<SettingsOnDeviceAiPageEntryElement> {
    return page.shadowRoot.querySelectorAll('settings-on-device-ai-page-entry');
  }

  function clickDeleteButton(index: number) {
    const deleteButton =
        getModelEntries()[index]!.shadowRoot.querySelector<HTMLElement>(
            '#delete');
    assertTrue(!!deleteButton);
    deleteButton.click();
  }

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy([]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    loadTimeData.overrideValues({
      showAiPage: true,
      showOnDeviceAiAdvancedSettings: true,
    });
    resetRouterForTesting();

    testBrowserProxy = new TestOnDeviceAiBrowserProxy();
    OnDeviceAiBrowserProxyImpl.setInstance(testBrowserProxy);
    await createPage();
  });

  teardown(function() {
    openWindowProxy.reset();
  });

  test('toggle default on and updates browser proxy', async function() {
    assertTrue(isVisible(page.$.onDeviceAiToggle));
    assertTrue(page.$.onDeviceAiToggle.checked);

    // Turning off requires confirmation: the toggle flips visually, but
    // nothing is committed until the dialog is confirmed.
    page.$.onDeviceAiToggle.click();
    await microtasksFinished();
    assertFalse(page.$.onDeviceAiToggle.checked);
    assertEquals(0, testBrowserProxy.getCallCount('setOnDeviceAiEnabled'));
    assertTrue(PrefService.getInstance()
                   .getPref<boolean>(ON_DEVICE_AI_PREF_KEY)
                   .value);

    const dialog =
        page.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);

    const whenClosed = eventToPromise('close', dialog);
    dialog.$.confirm.click();
    await whenClosed;
    assertFalse(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));
    await microtasksFinished();
    assertFalse(page.$.onDeviceAiToggle.checked);
    assertFalse(PrefService.getInstance()
                    .getPref<boolean>(ON_DEVICE_AI_PREF_KEY)
                    .value);
    assertNull(
        page.shadowRoot.querySelector('settings-simple-confirmation-dialog'));

    // Turning back on does not need confirmation.
    testBrowserProxy.resetResolver('setOnDeviceAiEnabled');
    page.$.onDeviceAiToggle.click();
    await microtasksFinished();
    assertTrue(page.$.onDeviceAiToggle.checked);
    assertNull(
        page.shadowRoot.querySelector('settings-simple-confirmation-dialog'));
    assertTrue(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));
    assertTrue(PrefService.getInstance()
                   .getPref<boolean>(ON_DEVICE_AI_PREF_KEY)
                   .value);
  });

  test('turnOffDialogCancelKeepsOnDeviceAiOn', async function() {
    page.$.onDeviceAiToggle.click();
    await microtasksFinished();
    assertFalse(page.$.onDeviceAiToggle.checked);
    const dialog =
        page.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!dialog);

    const whenClosed = eventToPromise('close', dialog);
    dialog.$.cancel.click();
    await whenClosed;
    await microtasksFinished();
    assertTrue(page.$.onDeviceAiToggle.checked);
    assertEquals(0, testBrowserProxy.getCallCount('setOnDeviceAiEnabled'));
    assertTrue(PrefService.getInstance()
                   .getPref<boolean>(ON_DEVICE_AI_PREF_KEY)
                   .value);
    assertNull(
        page.shadowRoot.querySelector('settings-simple-confirmation-dialog'));
  });

  test('turnOffDialogEscapeKeepsOnDeviceAiOn', async function() {
    page.$.onDeviceAiToggle.click();
    await microtasksFinished();
    const dialog =
        page.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!dialog);

    // Dismissing the dialog (e.g. with the Escape key) cancels the inner
    // cr-dialog and closes it without confirming.
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.dialog.cancel();
    await whenClosed;
    await microtasksFinished();
    assertTrue(page.$.onDeviceAiToggle.checked);
    assertEquals(0, testBrowserProxy.getCallCount('setOnDeviceAiEnabled'));
    assertNull(
        page.shadowRoot.querySelector('settings-simple-confirmation-dialog'));
  });

  test('disallowed by policy disables toggle', async function() {
    testBrowserProxy.reset();
    testBrowserProxy.setGetOnDeviceAiEnabledResponse({
      enabled: true,
      allowedByPolicy: false,
    });
    await createPage();

    assertTrue(page.$.onDeviceAiToggle.controlDisabled());
    assertTrue(page.$.onDeviceAiToggle.isPrefEnforced());
    assertFalse(page.$.onDeviceAiToggle.checked);
  });

  test('learnMoreLinkClicked', async function() {
    page.$.onDeviceAiToggle.dispatchEvent(
        new CustomEvent('sub-label-link-clicked'));

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString('onDeviceAiLearnMoreUrl'), url);
  });

  test('webUiListenerUpdatesToggle', async function() {
    assertTrue(page.$.onDeviceAiToggle.checked);

    webUIListenerCallback('on-device-ai-enabled-changed', {
      enabled: false,
      allowedByPolicy: true,
    });
    await microtasksFinished();
    assertFalse(page.$.onDeviceAiToggle.checked);
  });

  test('models section shows empty state when no models', function() {
    assertTrue(isVisible(page.shadowRoot.querySelector('#noModels')));
    assertFalse(!!page.shadowRoot.querySelector('#modelsList'));
    assertEquals(0, getModelEntries().length);
  });

  test('models section lists installed models', async function() {
    testBrowserProxy.reset();
    testBrowserProxy.setGetOnDeviceAiCapabilitiesResponse(
        [createGemmaCapability(), createAsrCapability()]);
    await createPage();

    assertFalse(!!page.shadowRoot.querySelector('#noModels'));
    const entries = getModelEntries();
    assertEquals(2, entries.length);
    assertDeepEquals(createGemmaCapability(), entries[0]!.capability);
    assertDeepEquals(createAsrCapability(), entries[1]!.capability);
  });

  test('models section hides downloadable models', async function() {
    testBrowserProxy.reset();
    testBrowserProxy.setGetOnDeviceAiCapabilitiesResponse([
      createGemmaCapability(),
      {...createAsrCapability(), status: OnDeviceModelStatus.DOWNLOADABLE},
    ]);
    await createPage();

    assertFalse(!!page.shadowRoot.querySelector('#noModels'));
    const entries = getModelEntries();
    assertEquals(1, entries.length);
    assertDeepEquals(createGemmaCapability(), entries[0]!.capability);
  });

  test(
      'models section lists downloading and updating models', async function() {
        const downloadingCapability =
            createGemmaCapability({status: OnDeviceModelStatus.DOWNLOADING});
        const updatingCapability =
            createAsrCapability({status: OnDeviceModelStatus.UPDATING});
        testBrowserProxy.reset();
        testBrowserProxy.setGetOnDeviceAiCapabilitiesResponse([
          downloadingCapability,
          updatingCapability,
          createGemmaCapability(
              {id: 3, status: OnDeviceModelStatus.DOWNLOADABLE}),
        ]);
        await createPage();

        assertFalse(!!page.shadowRoot.querySelector('#noModels'));
        const entries = getModelEntries();
        assertEquals(2, entries.length);
        assertDeepEquals(downloadingCapability, entries[0]!.capability);
        assertDeepEquals(updatingCapability, entries[1]!.capability);
      });

  test('delete button removes model', async function() {
    testBrowserProxy.reset();
    testBrowserProxy.setGetOnDeviceAiCapabilitiesResponse(
        [createGemmaCapability(), createAsrCapability()]);
    await createPage();
    assertEquals(2, getModelEntries().length);

    // Expand the second entry to ensure deleting the first preserves its state.
    getModelEntries()[1]!.$.expand.click();
    await microtasksFinished();
    assertTrue(getModelEntries()[1]!.$.collapse.opened);

    // The browser still lists a deleted model, but no longer as ready.
    testBrowserProxy.setDeleteOnDeviceAiCapabilityResponse([
      {...createGemmaCapability(), status: OnDeviceModelStatus.DOWNLOADABLE},
      createAsrCapability(),
    ]);
    clickDeleteButton(0);
    assertEquals(
        createGemmaCapability().id,
        await testBrowserProxy.whenCalled('deleteOnDeviceAiCapability'));
    await microtasksFinished();

    const entries = getModelEntries();
    assertEquals(1, entries.length);
    assertDeepEquals(createAsrCapability(), entries[0]!.capability);
    assertTrue(entries[0]!.$.collapse.opened);
  });

  test('deleting last model shows empty state', async function() {
    testBrowserProxy.reset();
    testBrowserProxy.setGetOnDeviceAiCapabilitiesResponse(
        [createGemmaCapability()]);
    await createPage();
    assertEquals(1, getModelEntries().length);

    testBrowserProxy.setDeleteOnDeviceAiCapabilityResponse([
      {...createGemmaCapability(), status: OnDeviceModelStatus.DOWNLOADABLE},
    ]);
    clickDeleteButton(0);
    await testBrowserProxy.whenCalled('deleteOnDeviceAiCapability');
    await microtasksFinished();

    assertEquals(0, getModelEntries().length);
    assertTrue(isVisible(page.shadowRoot.querySelector('#noModels')));
  });

  test('deleting model shows removed toast', async function() {
    testBrowserProxy.reset();
    testBrowserProxy.setGetOnDeviceAiCapabilitiesResponse(
        [createGemmaCapability(), createAsrCapability()]);
    await createPage();
    assertFalse(page.$.toast.open);

    testBrowserProxy.setDeleteOnDeviceAiCapabilityResponse([
      createGemmaCapability(),
      {...createAsrCapability(), status: OnDeviceModelStatus.DOWNLOADABLE},
    ]);
    clickDeleteButton(1);
    await testBrowserProxy.whenCalled('deleteOnDeviceAiCapability');
    await microtasksFinished();

    assertTrue(page.$.toast.open);
    assertEquals(
        loadTimeData.getStringF(
            'onDeviceAiModelRemovedToastLabel',
            createAsrCapability().categoryTitle),
        page.$.toast.textContent.trim());
  });
});
