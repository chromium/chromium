// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {DownloadsBrowserProxy, DownloadsBrowserProxyImpl, SettingsDownloadsPageElement} from 'chrome://settings/lazy_load.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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

  setup(function() {
    downloadsBrowserProxy = new TestDownloadsBrowserProxy();
    DownloadsBrowserProxyImpl.setInstance(downloadsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    downloadsPage = document.createElement('settings-downloads-page');
    document.body.appendChild(downloadsPage);

    // Page element must call 'initializeDownloads' upon attachment to the DOM.
    return downloadsBrowserProxy.whenCalled('initializeDownloads');
  });

  teardown(function() {
    downloadsPage.remove();
  });

  test('select downloads location', function() {
    const button = downloadsPage.shadowRoot!.querySelector<HTMLElement>(
        '#changeDownloadsPath');
    assertTrue(!!button);
    button!.click();
    button!.dispatchEvent(
        new CustomEvent('transitionend', {bubbles: true, composed: true}));
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
    downloadsPage.prefs = {
      download: {
        default_directory: {
          key: 'download.default_directory',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: downloadPath,
        },
      },
    };
  }

  function getDefaultDownloadPathString() {
    const pathElement =
        downloadsPage.shadowRoot!.querySelector('#defaultDownloadPath');
    assertTrue(!!pathElement);
    return pathElement!.textContent!.trim();
  }

  test('rewrite default download paths', async function() {
    setDefaultDownloadPathPref('downloads-path');
    const path =
        await downloadsBrowserProxy.whenCalled('getDownloadLocationText');
    assertEquals('downloads-path', path);
    flush();
    assertEquals('downloads-text', getDefaultDownloadPathString());
  });
  // </if>
});
