// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EDIT_STARTUP_URL_EVENT, StartupUrlsPageBrowserProxy, StartupUrlsPageBrowserProxyImpl} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// clang-format on

/** @implements {StartupUrlsPageBrowserProxy} */
class TestStartupUrlsPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'addStartupPage',
      'editStartupPage',
      'loadStartupPages',
      'removeStartupPage',
      'useCurrentPages',
      'validateStartupPage',
    ]);

    /** @private {boolean} */
    this.urlIsValid_ = true;
  }

  /** @param {boolean} isValid */
  setUrlValidity(isValid) {
    this.urlIsValid_ = isValid;
  }

  /** @override */
  addStartupPage(url) {
    this.methodCalled('addStartupPage', url);
    return Promise.resolve(this.urlIsValid_);
  }

  /** @override */
  editStartupPage(modelIndex, url) {
    this.methodCalled('editStartupPage', [modelIndex, url]);
    return Promise.resolve(this.urlIsValid_);
  }

  /** @override */
  loadStartupPages() {
    this.methodCalled('loadStartupPages');
  }

  /** @override */
  removeStartupPage(modelIndex) {
    this.methodCalled('removeStartupPage', modelIndex);
  }

  /** @override */
  useCurrentPages() {
    this.methodCalled('useCurrentPages');
  }

  /** @override */
  validateStartupPage(url) {
    this.methodCalled('validateStartupPage', url);
    return Promise.resolve(this.urlIsValid_);
  }
}

suite('StartupUrlDialog', function() {
  /** @type {?SettingsStartupUrlDialogElement} */
  let dialog = null;

  let browserProxy = null;

  /**
   * Triggers an 'input' event on the given text input field, which triggers
   * validation to occur.
   * @param {!CrInputElement} element
   */
  function pressSpace(element) {
    // The actual key code is irrelevant for these tests.
    keyEventOn(element, 'input', 32 /* space key code */);
  }

  setup(function() {
    browserProxy = new TestStartupUrlsPageBrowserProxy();
    StartupUrlsPageBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-startup-url-dialog');
  });

  teardown(function() {
    dialog.remove();
  });

  test('Initialization_Add', function() {
    document.body.appendChild(dialog);
    flush();
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
    pressSpace(inputElement);

    const url = await browserProxy.whenCalled('validateStartupPage');
    assertEquals(expectedUrl, url);
    assertTrue(actionButton.disabled);
    assertTrue(!!inputElement.invalid);

    browserProxy.setUrlValidity(true);
    browserProxy.resetResolver('validateStartupPage');
    pressSpace(inputElement);

    await browserProxy.whenCalled('validateStartupPage');
    assertFalse(actionButton.disabled);
    assertFalse(!!inputElement.invalid);
  });

  /**
   * Tests that the appropriate browser proxy method is called when the action
   * button is tapped.
   * @param {string} proxyMethodName
   */
  async function testProxyCalled(proxyMethodName) {
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
    pressSpace(inputElement);

    await browserProxy.whenCalled('validateStartupPage');
    keyEventOn(inputElement, 'keypress', 13, undefined, 'Enter');

    await browserProxy.whenCalled('addStartupPage');
  });
});

suite('StartupUrlsPage', function() {
  /** @type {?SettingsStartupUrlsPageElement} */
  let page = null;

  let browserProxy = null;

  setup(function() {
    browserProxy = new TestStartupUrlsPageBrowserProxy();
    StartupUrlsPageBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-startup-urls-page');
    page.prefs = {
      session: {
        restore_on_startup: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 5,
        },
      },
    };
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  // Test that the page is requesting information from the browser.
  test('Initialization', async function() {
    await browserProxy.whenCalled('loadStartupPages');
  });

  test('UseCurrentPages', async function() {
    const useCurrentPagesButton = page.$$('#useCurrentPages > a');
    assertTrue(!!useCurrentPagesButton);
    useCurrentPagesButton.click();
    await browserProxy.whenCalled('useCurrentPages');
  });

  test('AddPage_OpensDialog', async function() {
    const addPageButton = page.$$('#addPage > a');
    assertTrue(!!addPageButton);
    assertFalse(!!page.$$('settings-startup-url-dialog'));

    addPageButton.click();
    flush();
    assertTrue(!!page.$$('settings-startup-url-dialog'));
  });

  test('EditPage_OpensDialog', function() {
    assertFalse(!!page.$$('settings-startup-url-dialog'));
    page.fire(
        EDIT_STARTUP_URL_EVENT, {model: createSampleUrlEntry(), anchor: null});
    flush();
    assertTrue(!!page.$$('settings-startup-url-dialog'));
  });

  test('StartupPagesChanges_CloseOpenEditDialog', function() {
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
    page.fire(EDIT_STARTUP_URL_EVENT, {model: entry2, anchor: null});
    flush();

    assertTrue(!!page.$$('settings-startup-url-dialog'));
    webUIListenerCallback('update-startup-pages', [entry1]);
    flush();

    assertFalse(!!page.$$('settings-startup-url-dialog'));
  });

  test('StartupPages_WhenExtensionControlled', function() {
    assertFalse(!!page.get('prefs.session.startup_urls.controlledBy'));
    assertFalse(!!page.$$('extension-controlled-indicator'));
    assertTrue(!!page.$$('#addPage'));
    assertTrue(!!page.$$('#useCurrentPages'));

    page.set('prefs.session.startup_urls', {
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      controlledByName: 'Totally Real Extension',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      extensionId: 'mefmhpjnkplhdhmfmblilkgpkbjebmij',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 5,
    });
    flush();

    assertTrue(!!page.$$('extension-controlled-indicator'));
    assertFalse(!!page.$$('#addPage'));
    assertFalse(!!page.$$('#useCurrentPages'));
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
  /** @type {?SettingsStartupUrlEntryElement} */
  let element = null;

  let browserProxy = null;

  setup(function() {
    browserProxy = new TestStartupUrlsPageBrowserProxy();
    StartupUrlsPageBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    element = document.createElement('settings-startup-url-entry');
    element.model = createSampleUrlEntry();
    document.body.appendChild(element);
    flush();
  });

  teardown(function() {
    element.remove();
  });

  test('MenuOptions_Remove', async function() {
    element.editable = true;
    flush();

    // Bring up the popup menu.
    assertFalse(!!element.$$('cr-action-menu'));
    element.$$('#dots').click();
    flush();
    assertTrue(!!element.$$('cr-action-menu'));

    const removeButton = element.shadowRoot.querySelector('#remove');
    removeButton.click();
    const modelIndex = await browserProxy.whenCalled('removeStartupPage');
    assertEquals(element.model.modelIndex, modelIndex);
  });

  test('Editable', function() {
    assertFalse(!!element.editable);
    assertFalse(!!element.$$('#dots'));

    element.editable = true;
    flush();
    assertTrue(!!element.$$('#dots'));
  });
});
