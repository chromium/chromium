// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {ControlledButtonElement, DownloadsBrowserProxy, SettingsDownloadsPageElement} from 'chrome://settings/lazy_load.js';
import {DownloadsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
// <if expr="is_chromeos">
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

// </if>

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
// clang-format on

class TestDownloadsBrowserProxy extends TestBrowserProxy implements
    DownloadsBrowserProxy {
  constructor() {
    super([
      'initializeDownloads',
      'setDownloadsConnectionAccountLink',
      'selectDownloadLocation',
      'resetAutoOpenFileTypes',
      'getDownloadLocationText',
    ]);
  }

  initializeDownloads() {
    this.methodCalled('initializeDownloads');
  }

  setDownloadsConnectionAccountLink(enableLink: boolean) {
    this.methodCalled('setDownloadsConnectionAccountLink', enableLink);
  }

  selectDownloadLocation() {
    this.methodCalled('selectDownloadLocation');
  }

  resetAutoOpenFileTypes() {
    this.methodCalled('resetAutoOpenFileTypes');
  }

  // <if expr="is_chromeos">
  getDownloadLocationText(path: string) {
    this.methodCalled('getDownloadLocationText', path);
    return Promise.resolve('downloads-text');
  }
  // </if>
}

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'download.default_directory',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: '/path/to/downloads',
    },
    {
      key: 'download.prompt_for_download',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'download_bubble.partial_view_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
  ];
}

suite('DownloadsHandler', function() {
  let downloadsBrowserProxy: TestDownloadsBrowserProxy;
  let downloadsPage: SettingsDownloadsPageElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    loadTimeData.overrideValues({
      downloadBubblePartialViewControlledByPref: false,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    downloadsBrowserProxy = new TestDownloadsBrowserProxy();
    DownloadsBrowserProxyImpl.setInstance(downloadsBrowserProxy);

    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    downloadsPage = document.createElement('settings-downloads-page');
    document.body.appendChild(downloadsPage);

    // Page element must call 'initializeDownloads' upon attachment to the DOM.
    return downloadsBrowserProxy.whenCalled('initializeDownloads');
  });

  test('select downloads location', function() {
    const button = downloadsPage.shadowRoot.querySelector<HTMLElement>(
        '#changeDownloadsPath');
    assertTrue(!!button);
    button.click();
    return downloadsBrowserProxy.whenCalled('selectDownloadLocation');
  });

  test('controlled button pref enforcement', async function() {
    const button =
        downloadsPage.shadowRoot.querySelector<ControlledButtonElement>(
            '#changeDownloadsPath');
    assertTrue(!!button);
    const crButton = button.shadowRoot!.querySelector('cr-button');
    assertTrue(!!crButton);

    assertFalse(crButton.disabled);
    assertFalse(!!button.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'download.default_directory',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: '/path/to/downloads',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    }]);
    await microtasksFinished();

    assertTrue(crButton.disabled);
    assertTrue(!!button.shadowRoot!.querySelector('cr-policy-pref-indicator'));
  });

  test('openAdvancedDownloadsettings', async function() {
    let button = downloadsPage.shadowRoot.querySelector<HTMLElement>(
        '#resetAutoOpenFileTypes');
    assertFalse(!!button);

    webUIListenerCallback('auto-open-downloads-changed', true);
    await microtasksFinished();
    button = downloadsPage.shadowRoot.querySelector<HTMLElement>(
        '#resetAutoOpenFileTypes');
    assertTrue(!!button);

    button.click();
    await downloadsBrowserProxy.whenCalled('resetAutoOpenFileTypes');

    webUIListenerCallback('auto-open-downloads-changed', false);
    await microtasksFinished();
    button = downloadsPage.shadowRoot.querySelector<HTMLElement>(
        '#resetAutoOpenFileTypes');
    assertFalse(!!button);
  });

  // <if expr="is_chromeos">
  function setDefaultDownloadPathPref(downloadPath: string) {
    prefService.setPrefValue('download.default_directory', downloadPath);
  }

  function getDefaultDownloadPathString() {
    const pathElement =
        downloadsPage.shadowRoot.querySelector('#defaultDownloadPath');
    assertTrue(!!pathElement);
    return pathElement.textContent.trim();
  }

  test('rewrite default download paths', async function() {
    downloadsBrowserProxy.resetResolver('getDownloadLocationText');
    setDefaultDownloadPathPref('downloads-path');
    const path =
        await downloadsBrowserProxy.whenCalled('getDownloadLocationText');
    assertEquals('downloads-path', path);
    await microtasksFinished();
    assertEquals('downloads-text', getDefaultDownloadPathString());
  });
  // </if>

  test('showDownloadsToggleHidden', async function() {
    await microtasksFinished();
    const button = downloadsPage.shadowRoot.querySelector<HTMLElement>(
        '#showDownloadsToggle');
    assertFalse(!!button);
  });
});

suite('DownloadsHandlerWithBubblePartialView', function() {
  let downloadsBrowserProxy: TestDownloadsBrowserProxy;
  let downloadsPage: SettingsDownloadsPageElement;
  let prefService: PrefService;

  setup(async function() {
    loadTimeData.overrideValues({
      downloadBubblePartialViewControlledByPref: true,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    downloadsBrowserProxy = new TestDownloadsBrowserProxy();
    DownloadsBrowserProxyImpl.setInstance(downloadsBrowserProxy);

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    downloadsPage = document.createElement('settings-downloads-page');
    document.body.appendChild(downloadsPage);

    // Page element must call 'initializeDownloads' upon attachment to the DOM.
    return downloadsBrowserProxy.whenCalled('initializeDownloads');
  });

  test('showDownloadsToggleShown', function() {
    const button = downloadsPage.shadowRoot.querySelector<HTMLElement>(
        '#showDownloadsToggle');
    assertTrue(!!button);
  });

  test('showDownloadsToggleChangesPref', async function() {
    prefService.setPrefValue('download_bubble.partial_view_enabled', false);
    await microtasksFinished();
    assertFalse(
        prefService.getPref<boolean>('download_bubble.partial_view_enabled')
            .value);

    const button = downloadsPage.shadowRoot.querySelector<HTMLElement>(
        '#showDownloadsToggle');
    assertTrue(!!button);

    button.click();
    await microtasksFinished();
    assertTrue(
        prefService.getPref<boolean>('download_bubble.partial_view_enabled')
            .value);
  });
});
