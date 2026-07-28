// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {SettingsStartupUrlDialogElement, SettingsStartupUrlEntryElement, SettingsStartupUrlsPageElement} from 'chrome://settings/settings.js';
import {EDIT_STARTUP_URL_EVENT, PrefsBrowserProxy, PrefService, StartupUrlsPageBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestStartupUrlsPageBrowserProxy} from './test_startup_urls_page_browser_proxy.js';

// clang-format on


suite('StartupUrlDialog', function() {
  let dialog: SettingsStartupUrlDialogElement;
  let browserProxy: TestStartupUrlsPageBrowserProxy;

  /**
   * Triggers an 'input' event on the given text input field, which triggers
   * validation to occur.
   */
  function pressSpace(element: HTMLElement) {
    // The actual key code is irrelevant for these tests.
    keyEventOn(element, 'input', 32 /* space key code */);
  }

  setup(function() {
    browserProxy = new TestStartupUrlsPageBrowserProxy();
    StartupUrlsPageBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-startup-url-dialog');
  });

  teardown(function() {
    dialog.remove();
  });

  test('Initialization_Add', function() {
    document.body.appendChild(dialog);
    assertTrue(dialog.$.dialog.open);

    // Assert that the "Add" button is disabled.
    const actionButton = dialog.$.actionButton;
    assertTrue(!!actionButton);
    assertTrue(actionButton.disabled);

    // Assert that the text field is empty.
    const inputElement = dialog.$.url;
    assertTrue(!!inputElement);
    assertEquals('', inputElement.value);
  });

  test('Initialization_Edit', function() {
    dialog.model = createSampleUrlEntry();
    document.body.appendChild(dialog);
    assertTrue(dialog.$.dialog.open);

    // Assert that the "Edit" button is enabled.
    const actionButton = dialog.$.actionButton;
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);
    // Assert that the text field is pre-populated.
    const inputElement = dialog.$.url;
    assertTrue(!!inputElement);
    assertEquals(dialog.model.url, inputElement.value);
  });

  // Test that validation occurs as the user is typing, and that the action
  // button is updated accordingly.
  test('Validation', async function() {
    document.body.appendChild(dialog);

    const actionButton = dialog.$.actionButton;
    assertTrue(actionButton.disabled);
    const inputElement = dialog.$.url;

    const expectedUrl = 'dummy-foo.com';
    inputElement.value = expectedUrl;
    browserProxy.setUrlValidity(false);
    await microtasksFinished();
    pressSpace(inputElement);

    const url = await browserProxy.whenCalled('validateStartupPage');
    assertEquals(expectedUrl, url);
    await microtasksFinished();
    assertTrue(actionButton.disabled);
    assertTrue(inputElement.invalid);

    browserProxy.setUrlValidity(true);
    browserProxy.resetResolver('validateStartupPage');
    pressSpace(inputElement);

    await browserProxy.whenCalled('validateStartupPage');
    await microtasksFinished();
    assertFalse(actionButton.disabled);
    assertFalse(inputElement.invalid);
  });

  /**
   * Tests that the appropriate browser proxy method is called when the action
   * button is tapped.
   */
  async function testProxyCalled(proxyMethodName: string) {
    const actionButton = dialog.$.actionButton;
    actionButton.disabled = false;

    // Test that the dialog remains open if the user somehow manages to submit
    // an invalid URL.
    browserProxy.setUrlValidity(false);
    actionButton.click();
    await browserProxy.whenCalled(proxyMethodName);
    assertTrue(dialog.$.dialog.open);

    // Test that dialog is closed if the user submits a valid URL.
    browserProxy.setUrlValidity(true);
    browserProxy.resetResolver(proxyMethodName);
    actionButton.click();
    await browserProxy.whenCalled(proxyMethodName);
    assertFalse(dialog.$.dialog.open);
  }

  test('AddStartupPage', async function() {
    document.body.appendChild(dialog);
    await testProxyCalled('addStartupPage');
  });

  test('EditStartupPage', async function() {
    dialog.model = createSampleUrlEntry();
    document.body.appendChild(dialog);
    await testProxyCalled('editStartupPage');
  });

  test('Enter key submits', async function() {
    document.body.appendChild(dialog);

    // Input a URL and force validation.
    const inputElement = dialog.$.url;
    inputElement.value = 'foo.com';
    await microtasksFinished();
    pressSpace(inputElement);

    await browserProxy.whenCalled('validateStartupPage');
    // Wait for the action button to become enabled.
    await microtasksFinished();
    keyEventOn(inputElement, 'keypress', 13, undefined, 'Enter');

    await browserProxy.whenCalled('addStartupPage');
  });
});

suite('StartupUrlsPage', function() {
  let page: SettingsStartupUrlsPageElement;
  let browserProxy: TestStartupUrlsPageBrowserProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
    return [
      {
        key: 'session.startup_urls',
        type: chrome.settingsPrivate.PrefType.LIST,
        value: [],
      },
    ];
  }

  setup(async function() {
    browserProxy = new TestStartupUrlsPageBrowserProxy();
    StartupUrlsPageBrowserProxyImpl.setInstance(browserProxy);

    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-startup-urls-page');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  // Test that the page is requesting information from the browser.
  test('Initialization', async function() {
    await browserProxy.whenCalled('loadStartupPages');
  });

  test('UseCurrentPages', async function() {
    const useCurrentPagesButton =
        page.shadowRoot.querySelector<HTMLElement>('#useCurrentPages > a');
    assertTrue(!!useCurrentPagesButton);
    useCurrentPagesButton.click();
    await browserProxy.whenCalled('useCurrentPages');
  });

  test('AddPage_OpensDialog', async function() {
    const addPageButton =
        page.shadowRoot.querySelector<HTMLElement>('#addPage > a');
    assertTrue(!!addPageButton);
    assertFalse(!!page.shadowRoot.querySelector('settings-startup-url-dialog'));

    addPageButton.click();
    await microtasksFinished();
    assertTrue(!!page.shadowRoot.querySelector('settings-startup-url-dialog'));
  });

  test('EditPage_OpensDialog', async function() {
    assertFalse(!!page.shadowRoot.querySelector('settings-startup-url-dialog'));
    page.fire(EDIT_STARTUP_URL_EVENT, {
      model: createSampleUrlEntry(),
      anchor: null,
    });
    await microtasksFinished();
    assertTrue(!!page.shadowRoot.querySelector('settings-startup-url-dialog'));
  });

  test('StartupPagesChanges_CloseOpenEditDialog', async function() {
    const entry1 = {
      modelIndex: 2,
      title: 'Test page 1',
      tooltip: 'test tooltip',
      url: 'chrome://bar',
    };

    const entry2 = {
      modelIndex: 2,
      title: 'Test page 2',
      tooltip: 'test tooltip',
      url: 'chrome://foo',
    };

    webUIListenerCallback('update-startup-pages', [entry1, entry2]);
    page.fire(EDIT_STARTUP_URL_EVENT, {
      model: entry2,
      anchor: null,
    });
    await microtasksFinished();

    assertTrue(!!page.shadowRoot.querySelector('settings-startup-url-dialog'));
    webUIListenerCallback('update-startup-pages', [entry1]);
    await microtasksFinished();

    assertFalse(!!page.shadowRoot.querySelector('settings-startup-url-dialog'));
  });

  test('StartupPages_WhenExtensionControlled', async function() {
    assertFalse(!!prefService.getPref('session.startup_urls').controlledBy);
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));
    assertTrue(!!page.shadowRoot.querySelector('#addPage'));
    assertTrue(!!page.shadowRoot.querySelector('#useCurrentPages'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'session.startup_urls',
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      controlledByName: 'Totally Real Extension',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      extensionId: 'mefmhpjnkplhdhmfmblilkgpkbjebmij',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: [],
    }]);
    await microtasksFinished();

    assertTrue(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));
    assertFalse(!!page.shadowRoot.querySelector('#addPage'));
    assertFalse(!!page.shadowRoot.querySelector('#useCurrentPages'));
  });
});

/** @return {!StartupPageInfo} */
function createSampleUrlEntry() {
  return {
    modelIndex: 2,
    title: 'Test page',
    tooltip: 'test tooltip',
    url: 'chrome://foo',
  };
}

suite('StartupUrlEntry', function() {
  let element: SettingsStartupUrlEntryElement;
  let browserProxy: TestStartupUrlsPageBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestStartupUrlsPageBrowserProxy();
    StartupUrlsPageBrowserProxyImpl.setInstance(browserProxy);
    element = document.createElement('settings-startup-url-entry');
    element.model = createSampleUrlEntry();
    document.body.appendChild(element);
  });

  test('MenuOptions_Remove', async function() {
    element.editable = true;
    await microtasksFinished();

    // Bring up the popup menu.
    assertFalse(!!element.shadowRoot.querySelector('cr-action-menu'));
    element.shadowRoot.querySelector<HTMLElement>('#dots')!.click();
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('cr-action-menu'));

    const removeButton =
        element.shadowRoot.querySelector<HTMLElement>('#remove');
    removeButton!.click();
    const modelIndex = await browserProxy.whenCalled('removeStartupPage');
    assertEquals(element.model.modelIndex, modelIndex);
  });

  test('Editable', async function() {
    assertFalse(element.editable);
    assertFalse(!!element.shadowRoot.querySelector('#dots'));

    element.editable = true;
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('#dots'));
  });

  test('MoreActionsButtonTitle', async function() {
    element.editable = true;
    await microtasksFinished();
    const dots = element.shadowRoot.querySelector<HTMLElement>('#dots');
    assertTrue(!!dots);
    const title = dots.getAttribute('title');
    assertTrue(!!title);
    assertTrue(title.includes(element.model.title));
  });
});
