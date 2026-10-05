// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsBrowserProxyImpl, SiteSettingsMixin, SiteSettingSource} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createRawSiteException} from './test_util.js';

suite('SiteSettingsMixin', function() {
  const TestElementBase = SiteSettingsMixin(PolymerElement);

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-site-settings-mixin';
    }

    static get template() {
      return html`<div id="category">[[category]]</div>`;
    }
  }
  customElements.define(TestElement.is, TestElement);

  let testElement: TestElement;
  let browserProxy: TestSiteSettingsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    testElement =
        document.createElement('test-site-settings-mixin') as TestElement;
    document.body.appendChild(testElement);
  });

  test('PropertiesAndBrowserProxy', function() {
    assertEquals(browserProxy, testElement.browserProxy);
    assertEquals(undefined, testElement.category);

    testElement.category = ContentSettingsTypes.COOKIES;
    assertEquals(
        ContentSettingsTypes.COOKIES,
        testElement.shadowRoot!.querySelector('#category')!.textContent);
  });

  test('SanitizePort', function() {
    assertEquals('', testElement.sanitizePort(''));
    assertEquals(
        'https://example.com',
        testElement.sanitizePort('https://example.com:443'));
    assertEquals(
        'http://example.com',
        testElement.sanitizePort('http://example.com:80'));
    assertEquals('example.com', testElement.sanitizePort('example.com:80'));
    assertEquals(
        'https://example.com:8080',
        testElement.sanitizePort('https://example.com:8080'));
    assertEquals(
        'http://example.com:443',
        testElement.sanitizePort('http://example.com:443'));
  });

  test('ToUrl', function() {
    assertNull(testElement.toUrl(''));

    const url1 = testElement.toUrl('https://example.com:8080');
    assertTrue(!!url1);
    assertEquals('https://example.com:8080/', url1.href);

    const url2 = testElement.toUrl('*://[*.]example.com');
    assertTrue(!!url2);
    assertEquals('http://example.com/', url2.href);

    const url3 = testElement.toUrl('chrome-extension://id');
    assertTrue(!!url3);
    assertEquals('chrome-extension://id/', url3.href);
  });

  test('OriginRepresentation', function() {
    assertEquals('', testElement.originRepresentation(''));
    assertEquals(
        'example.com', testElement.originRepresentation('https://example.com'));
    assertEquals(
        'example.com:8080',
        testElement.originRepresentation('https://example.com:8080'));
    assertEquals(
        'example.com', testElement.originRepresentation('*://[*.]example.com'));
    assertEquals('', testElement.originRepresentation('http://'));
  });

  test('ExpandSiteException', function() {
    const prefException = testElement.expandSiteException(
        createRawSiteException('https://example.com', {
          type: ContentSettingsTypes.GEOLOCATION,
          setting: ContentSetting.ALLOW,
          source: SiteSettingSource.PREFERENCE,
        }));
    assertEquals(ContentSettingsTypes.GEOLOCATION, prefException.category);
    assertEquals('https://example.com', prefException.origin);
    assertNull(prefException.enforcement);
    assertEquals(
        chrome.settingsPrivate.ControlledBy.PRIMARY_USER,
        prefException.controlledBy);

    const extensionException = testElement.expandSiteException(
        createRawSiteException('https://example.com', {
          source: SiteSettingSource.EXTENSION,
        }));
    assertEquals(
        chrome.settingsPrivate.Enforcement.ENFORCED,
        extensionException.enforcement);
    assertEquals(
        chrome.settingsPrivate.ControlledBy.EXTENSION,
        extensionException.controlledBy);

    const hostedAppException = testElement.expandSiteException(
        createRawSiteException('https://example.com', {
          source: SiteSettingSource.HOSTED_APP,
        }));
    assertEquals(
        chrome.settingsPrivate.Enforcement.ENFORCED,
        hostedAppException.enforcement);
    assertEquals(
        chrome.settingsPrivate.ControlledBy.EXTENSION,
        hostedAppException.controlledBy);

    const policyException = testElement.expandSiteException(
        createRawSiteException('https://example.com', {
          source: SiteSettingSource.POLICY,
        }));
    assertEquals(
        chrome.settingsPrivate.Enforcement.ENFORCED,
        policyException.enforcement);
    assertEquals(
        chrome.settingsPrivate.ControlledBy.USER_POLICY,
        policyException.controlledBy);
  });
});
