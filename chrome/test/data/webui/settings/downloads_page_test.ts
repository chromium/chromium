// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import type {DownloadsBrowserProxy, SettingsDownloadsPageElement} from 'chrome://settings/lazy_load.js';
import {DownloadsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
// <if expr="chromeos_ash">
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
// </if>

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
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

  // <if expr="chromeos_ash">
  getDownloadLocationText(path: string) {
    this.methodCalled('getDownloadLocationText', path);
    return Promise.resolve('downloads-text');
  }
  // </if>
}

suite('DownloadsHandler', function() {
  let downloadsBrowserProxy: TestDownloadsBrowserProxy;
  let downloadsPage: SettingsDownloadsPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    downloadsBrowserProxy = new TestDownloadsBrowserProxy();
    DownloadsBrowserProxyImpl.setInstance(downloadsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    downloadsPage = document.createElement('settings-downloads-page');
    downloadsPage.prefs = settingsPrefs.prefs;
    document.body.appendChild(downloadsPage);

    // Page element must call 'initializeDownloads' upon attachment to the DOM.
    return downloadsBrowserProxy.whenCalled('initializeDownloads');
  });

  test('select downloads location', function() {
    const button = downloadsPage.shadowRoot!.querySelector<HTMLElement>(
        '#changeDownloadsPath');
    assertTrue(!!button);
    button.click();
    return downloadsBrowserProxy.whenCalled('selectDownloadLocation');
  });

  test('openAdvancedDownloadsettings', async function() {
    let button = downloadsPage.shadowRoot!.querySelector<HTMLElement>(
        '#resetAutoOpenFileTypes');
    assertFalse(!!button);

    webUIListenerCallback('auto-open-downloads-changed', true);
    flush();
    button = downloadsPage.shadowRoot!.querySelector<HTMLElement>(
        '#resetAutoOpenFileTypes');
    assertTrue(!!button);

    button!.click();
    await downloadsBrowserProxy.whenCalled('resetAutoOpenFileTypes');

    webUIListenerCallback('auto-open-downloads-changed', false);
    flush();
    button = downloadsPage.shadowRoot!.querySelector<HTMLElement>(
        '#resetAutoOpenFileTypes');
    assertFalse(!!button);
  });

  // <if expr="chromeos_ash">
  function setDefaultDownloadPathPref(downloadPath: string) {
    downloadsPage.setPrefValue('download.default_directory', downloadPath);
  }

  function getDefaultDownloadPathString() {
    const pathElement =
        downloadsPage.shadowRoot!.querySelector('#defaultDownloadPath');
    assertTrue(!!pathElement);
    return pathElement!.textContent!.trim();
  }

  test('rewrite default download paths', async function() {
    downloadsBrowserProxy.resetResolver('getDownloadLocationText');
    setDefaultDownloadPathPref('downloads-path');
    const path =
        await downloadsBrowserProxy.whenCalled('getDownloadLocationText');
    assertEquals('downloads-path', path);
    flush();
    assertEquals('downloads-text', getDefaultDownloadPathString());
  });
  // </if>

  test('showDownloadsToggleHidden', function() {
    const button =
        downloadsPage.querySelector<HTMLElement>('#showDownloadsToggle');
    assertFalse(!!button);
  });
});

suite('DownloadsHandlerWithBubblePartialView', function() {
  let downloadsBrowserProxy: TestDownloadsBrowserProxy;
  let downloadsPage: SettingsDownloadsPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      downloadBubblePartialViewControlledByPref: true,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    downloadsBrowserProxy = new TestDownloadsBrowserProxy();
    DownloadsBrowserProxyImpl.setInstance(downloadsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(settingsPrefs);
    downloadsPage = document.createElement('settings-downloads-page');
    downloadsPage.prefs = settingsPrefs.prefs;
    document.body.appendChild(downloadsPage);

    // Page element must call 'initializeDownloads' upon attachment to the DOM.
    return downloadsBrowserProxy.whenCalled('initializeDownloads');
  });

  test('showDownloadsToggleShown', function() {
    const button = downloadsPage.shadowRoot!.querySelector<HTMLElement>(
        '#showDownloadsToggle');
    assertTrue(!!button);
  });

  test('showDownloadsToggleChangesPref', async function() {
    downloadsPage.setPrefValue('download_bubble.partial_view_enabled', false);
    await flushTasks();
    assertFalse(
        downloadsPage.getPref('download_bubble.partial_view_enabled').value);

    const button = downloadsPage.shadowRoot!.querySelector<HTMLElement>(
        '#showDownloadsToggle');
    assertTrue(!!button);

    button.click();
    await flushTasks();
    assertTrue(
        downloadsPage.getPref('download_bubble.partial_view_enabled').value);
  });
});
