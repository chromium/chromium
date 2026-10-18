// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsBrowserProxyImpl, SiteSettingsMixinLit, SiteSettingSource} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createRawSiteException} from './test_util.js';

suite('SiteSettingsMixinLit', function() {
  const TestElementBase = SiteSettingsMixinLit(CrLitElement);

  class TestSiteSettingsMixinLitElement extends TestElementBase {
    static get is() {
      return 'test-site-settings-mixin-lit';
    }

    override render() {
      return html`<div id="category">${this.category}</div>`;
    }
  }
  customElements.define(
      TestSiteSettingsMixinLitElement.is, TestSiteSettingsMixinLitElement);

  let testElement: TestSiteSettingsMixinLitElement;
  let browserProxy: TestSiteSettingsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    testElement = document.createElement('test-site-settings-mixin-lit') as
        TestSiteSettingsMixinLitElement;
    document.body.appendChild(testElement);
  });

  test('PropertiesAndBrowserProxy', async function() {
    assertEquals(browserProxy, testElement.browserProxy);
    assertEquals(undefined, testElement.category);

    testElement.category = ContentSettingsTypes.GEOLOCATION;
    await microtasksFinished();
    assertEquals(
        ContentSettingsTypes.GEOLOCATION,
        testElement.shadowRoot.querySelector('#category')!.textContent);
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
    assertNull(testElement.toUrl('http://'));
    assertNull(testElement.toUrl('http://[invalid]'));

    const url1 = testElement.toUrl('https://example.com:8080');
    assertTrue(!!url1);
    assertEquals('https://example.com:8080/', url1.href);

    const url2 = testElement.toUrl('*://example.com');
    assertTrue(!!url2);
    assertEquals('http://example.com/', url2.href);

    const url3 = testElement.toUrl('[*.]example.com');
    assertTrue(!!url3);
    assertEquals('http://example.com/', url3.href);

    const url4 = testElement.toUrl('https://[*.]example.com');
    assertTrue(!!url4);
    assertEquals('https://example.com/', url4.href);

    const url5 = testElement.toUrl('chrome-extension://id');
    assertTrue(!!url5);
    assertEquals('chrome-extension://id/', url5.href);

    const url6 = testElement.toUrl('example.com?redirect=https://foo.com');
    assertTrue(!!url6);
    assertEquals('http://example.com/?redirect=https://foo.com', url6.href);
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
    for (const source of Object.values(SiteSettingSource)) {
      const exception = testElement.expandSiteException(
          createRawSiteException('https://example.com', {
            type: ContentSettingsTypes.GEOLOCATION,
            setting: ContentSetting.ALLOW,
            source,
          }));
      assertEquals(ContentSettingsTypes.GEOLOCATION, exception.category);
      assertEquals('https://example.com', exception.origin);

      if (source === SiteSettingSource.EXTENSION ||
          source === SiteSettingSource.HOSTED_APP) {
        assertEquals(
            chrome.settingsPrivate.Enforcement.ENFORCED, exception.enforcement);
        assertEquals(
            chrome.settingsPrivate.ControlledBy.EXTENSION,
            exception.controlledBy);
      } else if (source === SiteSettingSource.POLICY) {
        assertEquals(
            chrome.settingsPrivate.Enforcement.ENFORCED, exception.enforcement);
        assertEquals(
            chrome.settingsPrivate.ControlledBy.USER_POLICY,
            exception.controlledBy);
      } else {
        assertNull(exception.enforcement);
        assertEquals(
            chrome.settingsPrivate.ControlledBy.PRIMARY_USER,
            exception.controlledBy);
      }
    }
  });
});
