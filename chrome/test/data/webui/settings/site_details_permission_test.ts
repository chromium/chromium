// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://webui-test/cr_elements/cr_policy_strings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SiteDetailsPermissionElement} from 'chrome://settings/lazy_load.js';
import {ChooserType, ContentSetting, ContentSettingsTypes, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createRawChooserException, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';
// clang-format on

/** @fileoverview Suite of tests for site-details. */
suite('SiteDetailsPermission', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: SiteDetailsPermissionElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  /**
   * An example pref with only camera allowed.
   */
  let prefs: SiteSettingsPref;

  // Initialize a site-details-permission before each test.
  setup(function() {
    prefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.CAMERA, createDefaultContentSetting({
              setting: ContentSetting.ALLOW,
            }))],
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.CAMERA,
            [createRawSiteException('https://www.example.com')])]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-details-permission');
    document.body.appendChild(testElement);
  });

  async function validatePermissionFlipWorks(
      origin: string, expectedContentSetting: ContentSetting) {
    browserProxy.resetResolver('setOriginPermissions');

    // Simulate permission change initiated by the user.
    testElement.$.permission.value = expectedContentSetting;
    testElement.$.permission.dispatchEvent(new CustomEvent('change'));

    const [site, category, setting] =
        await browserProxy.whenCalled('setOriginPermissions');
    assertEquals(origin, site);
    assertDeepEquals(testElement.category, category);
    assertEquals(expectedContentSetting, setting);
  }

  test('camera category', async function() {
    const origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = ContentSettingsTypes.CAMERA;
    testElement.label = 'Camera';
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: '',
      source: SiteSettingSource.PREFERENCE,
    });

    assertFalse(testElement.$.details.hidden);

    const header =
        testElement.$.details.querySelector<HTMLElement>('#permissionHeader')!;
    assertEquals(
        'Camera', header.innerText!.trim(),
        'Widget should be labelled correctly');

    // Flip the permission and validate that prefs stay in sync.
    await validatePermissionFlipWorks(origin, ContentSetting.ALLOW);
    await validatePermissionFlipWorks(origin, ContentSetting.BLOCK);
    await validatePermissionFlipWorks(origin, ContentSetting.ALLOW);
    await validatePermissionFlipWorks(origin, ContentSetting.DEFAULT);
  });

  test('default string is correct', async function() {
    const origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = ContentSettingsTypes.CAMERA;
    testElement.label = 'Camera';
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: '',
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.PREFERENCE,
    });

    let args = await browserProxy.whenCalled('getDefaultValueForContentType');
    // Check getDefaultValueForContentType was called for camera category.
    assertEquals(ContentSettingsTypes.CAMERA, args);

    // The default option will always be the first in the menu.
    assertEquals(
        'Allow (default)', testElement.$.permission.options[0]!.text,
        'Default setting string should match prefs');
    browserProxy.resetResolver('getDefaultValueForContentType');
    let defaultPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.CAMERA,
            createDefaultContentSetting({setting: ContentSetting.BLOCK}))],
        []);
    browserProxy.setPrefs(defaultPrefs);

    args = await browserProxy.whenCalled('getDefaultValueForContentType');
    assertEquals(ContentSettingsTypes.CAMERA, args);
    assertEquals(
        'Block (default)', testElement.$.permission.options[0]!.text,
        'Default setting string should match prefs');
    browserProxy.resetResolver('getDefaultValueForContentType');
    defaultPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.CAMERA, createDefaultContentSetting())],
        []);
    browserProxy.setPrefs(defaultPrefs);

    args = await browserProxy.whenCalled('getDefaultValueForContentType');
    assertEquals(ContentSettingsTypes.CAMERA, args);
    assertEquals(
        'Ask (default)', testElement.$.permission.options[0]!.text,
        'Default setting string should match prefs');
  });

  test('info string is correct', function() {
    const origin = 'https://www.example.com';
    testElement.category = ContentSettingsTypes.CAMERA;

    // Strings that should be shown for the permission sources that don't depend
    // on the ContentSetting value.
    const permissionSourcesNoSetting: Map<SiteSettingSource, string> = new Map([
      [SiteSettingSource.DEFAULT, ''],
      [SiteSettingSource.EMBARGO, 'Automatically blocked'],
      [SiteSettingSource.INSECURE_ORIGIN, 'Blocked to protect your privacy'],
      [
        SiteSettingSource.KILL_SWITCH,
        'Temporarily blocked to protect your security',
      ],
      [SiteSettingSource.PREFERENCE, ''],
    ]);

    for (const [source, str] of permissionSourcesNoSetting) {
      testElement.site = createRawSiteException(origin, {
        origin: origin,
        embeddingOrigin: origin,
        setting: ContentSetting.BLOCK,
        source,
      });
      assertEquals(
          str +
              (str.length === 0 ? 'Block (default)\nAllow\nBlock\nAsk' :
                                  '\nBlock (default)\nAllow\nBlock\nAsk'),
          testElement.$.permissionItem.innerText.trim());
      assertEquals(
          str !== '',
          testElement.$.permissionItem.classList.contains('two-line'));

      if (source !== SiteSettingSource.DEFAULT &&
          source !== SiteSettingSource.PREFERENCE &&
          source !== SiteSettingSource.EMBARGO) {
        assertTrue(testElement.$.permission.disabled);
      } else {
        assertFalse(testElement.$.permission.disabled);
      }
    }

    // Permissions that have been set by extensions.
    const extensionSourceStrings: Map<ContentSetting, string> = new Map([
      [ContentSetting.ALLOW, 'Allowed by an extension'],
      [ContentSetting.BLOCK, 'Blocked by an extension'],
      [ContentSetting.ASK, 'Setting controlled by an extension'],
    ]);

    for (const [setting, str] of extensionSourceStrings) {
      testElement.site = createRawSiteException(origin, {
        origin: origin,
        embeddingOrigin: origin,
        setting,
        source: SiteSettingSource.EXTENSION,
      });
      assertEquals(
          str + '\nBlock (default)\nAllow\nBlock\nAsk',
          testElement.$.permissionItem.innerText.trim());
      assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
      assertTrue(testElement.$.permission.disabled);
      assertEquals(setting, testElement.$.permission.value);
    }

    // Permissions that have been set by enterprise policy.
    const policySourceStrings: Map<ContentSetting, string> = new Map([
      [ContentSetting.ALLOW, 'Allowed by your administrator'],
      [ContentSetting.ASK, 'Setting controlled by your administrator'],
      [ContentSetting.BLOCK, 'Blocked by your administrator'],
    ]);

    for (const [setting, str] of policySourceStrings) {
      testElement.site = createRawSiteException(origin, {
        origin: origin,
        embeddingOrigin: origin,
        setting,
        source: SiteSettingSource.POLICY,
      });
      assertEquals(
          str + '\nBlock (default)\nAllow\nBlock\nAsk',
          testElement.$.permissionItem.innerText.trim());
      assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
      assertTrue(testElement.$.permission.disabled);
      assertEquals(setting, testElement.$.permission.value);
    }

    // Finally, check if changing the source from a non-user-controlled setting
    // (policy) back to a user-controlled one re-enables the control.
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ASK,
      source: SiteSettingSource.DEFAULT,
    });
    assertEquals(
        'Ask (default)\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertFalse(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);
  });

  test('info string correct for ads', function() {
    const origin = 'https://www.example.com';
    testElement.category = ContentSettingsTypes.ADS;
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.BLOCK,
      source: SiteSettingSource.ADS_FILTER_BLACKLIST,
    });
    assertEquals(
        'Site shows intrusive or misleading ads' +
            '\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);

    // Check the string that shows when ads is blocked but not blacklisted.
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.BLOCK,
      source: SiteSettingSource.PREFERENCE,
    });
    assertEquals(
        'Block if site shows intrusive or misleading ads' +
            '\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);

    // Ditto for default block settings.
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.BLOCK,
      source: SiteSettingSource.DEFAULT,
    });
    assertEquals(
        'Block if site shows intrusive or misleading ads' +
            '\nBlock (default)\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);

    // Allowing ads for unblacklisted sites shows nothing.
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.PREFERENCE,
    });
    assertEquals(
        'Block (default)\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertFalse(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);
  });

  test('info string correct for allowlisted source', function() {
    const origin = 'chrome://test';
    testElement.category = ContentSettingsTypes.NOTIFICATIONS;
    testElement.$.details.hidden = false;
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.ALLOWLIST,
    });
    assertEquals(
        'Allowlisted internally\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertTrue(testElement.$.permission.disabled);
  });

  test('info string correct for system block', async function() {
    const origin = 'chrome://test';
    const categoryList = [
      ContentSettingsTypes.CAMERA,
      ContentSettingsTypes.MIC,
      ContentSettingsTypes.GEOLOCATION,
    ];
    for (const category of categoryList) {
      for (const disabled of [true, false]) {
        testElement.category = category;
        testElement.$.details.hidden = false;
        testElement.site = createRawSiteException(origin, {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.ALLOW,
          source: SiteSettingSource.PREFERENCE,
        });

        const blockedPermissions = disabled ? [category] : [];
        webUIListenerCallback('osGlobalPermissionChanged', blockedPermissions);

        const warningElement =
            testElement.$.permissionItem.querySelector('#permissionSecondary');
        assertTrue(!!warningElement);
        if (!disabled) {
          assertTrue(warningElement.hasAttribute('hidden'));
          return;
        }

        assertFalse(warningElement.hasAttribute('hidden'));

        const sensor =
            (() => {
              switch (category) {
                case ContentSettingsTypes.CAMERA:
                  return 'camera';
                case ContentSettingsTypes.MIC:
                  return 'microphone';
                case ContentSettingsTypes.GEOLOCATION:
                  return 'location';
                default:
                  throw new Error(`Unsupported category type: ${category}`);
              }
            })() as string;

        const variant = warningElement.innerHTML.includes('Chromium') ?
            'Chromium' :
            'Chrome';

        // Check the visible text of the warning.
        assertEquals(
            `To use your ${sensor}, give ${variant} access in system settings`,
            warningElement.textContent);

        const linkElement = testElement.$.permissionItem.querySelector(
            '#openSystemSettingsLink');
        assertTrue(!!linkElement);
        // Check that the link covers the right part of the warning.
        assertEquals('system settings', linkElement.innerHTML);
        // This is needed for the <a> to look like a link.
        assertEquals('#', linkElement.getAttribute('href'));
        // This is needed for accessibility. First letter if the sensor name is
        // capitalized.
        assertEquals(
            `System Settings: ${sensor.replace(/^\w/, (c) => c.toUpperCase())}`,
            linkElement.getAttribute('aria-label'));

        browserProxy.resetResolver('openSystemPermissionSettings');
        linkElement.dispatchEvent(new MouseEvent('click'));
        await browserProxy.whenCalled('openSystemPermissionSettings')
            .then((contentType: string) => {
              assertEquals(category, contentType);
            });
      }
    }
  });

  test('sound setting default string is correct', async function() {
    const origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = ContentSettingsTypes.SOUND;
    testElement.label = 'Sound';
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: '',
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.PREFERENCE,
    });

    let args = await browserProxy.whenCalled('getDefaultValueForContentType');
    // Check getDefaultValueForContentType was called for sound category.
    assertEquals(ContentSettingsTypes.SOUND, args);

    // The default option will always be the first in the menu.
    assertEquals(
        'Allow (default)', testElement.$.permission.options[0]!.text,
        'Default setting string should match prefs');
    browserProxy.resetResolver('getDefaultValueForContentType');
    let defaultPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.SOUND,
            createDefaultContentSetting({setting: ContentSetting.BLOCK}))],
        []);
    browserProxy.setPrefs(defaultPrefs);

    args = await browserProxy.whenCalled('getDefaultValueForContentType');
    assertEquals(ContentSettingsTypes.SOUND, args);
    assertEquals(
        'Mute (default)', testElement.$.permission.options[0]!.text,
        'Default setting string should match prefs');
    browserProxy.resetResolver('getDefaultValueForContentType');
    testElement.useAutomaticLabel = true;
    defaultPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.SOUND,
            createDefaultContentSetting({setting: ContentSetting.ALLOW}))],
        []);
    browserProxy.setPrefs(defaultPrefs);

    args = await browserProxy.whenCalled('getDefaultValueForContentType');
    assertEquals(ContentSettingsTypes.SOUND, args);
    assertEquals(
        'Automatic (default)', testElement.$.permission.options[0]!.text,
        'Default setting string should match prefs');
  });

  test('sound setting block string is correct', async function() {
    const origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = ContentSettingsTypes.SOUND;
    testElement.label = 'Sound';
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: '',
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.PREFERENCE,
    });

    const args = await browserProxy.whenCalled('getDefaultValueForContentType');

    // Check getDefaultValueForContentType was called for sound category.
    assertEquals(ContentSettingsTypes.SOUND, args);

    // The block option will always be the third in the menu.
    assertEquals(
        'Mute', testElement.$.permission.options[2]!.text,
        'Block setting string should match prefs');
  });

  test('ASK can be chosen as a preference by users', function() {
    const origin = 'https://www.example.com';
    testElement.category = ContentSettingsTypes.USB_DEVICES;
    testElement.label = 'USB';
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ASK,
      source: SiteSettingSource.PREFERENCE,
    });

    // In addition to the assertions below, the main goal of this test is to
    // ensure we do not hit any assertions when choosing ASK as a setting.
    assertEquals(testElement.$.permission.value, ContentSetting.ASK);
    assertFalse(testElement.$.permission.disabled);
    assertFalse(
        testElement.$.permission.querySelector<HTMLElement>('#ask')!.hidden);
  });

  test(
      'Bluetooth scanning: ASK/BLOCK can be chosen as a preference by users',
      function() {
        const origin = 'https://www.example.com';
        testElement.category = ContentSettingsTypes.BLUETOOTH_SCANNING;
        testElement.label = 'Bluetooth-scanning';
        testElement.site = createRawSiteException(origin, {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.ASK,
          source: SiteSettingSource.PREFERENCE,
        });

        // In addition to the assertions below, the main goal of this test is to
        // ensure we do not hit any assertions when choosing ASK as a setting.
        assertEquals(testElement.$.permission.value, ContentSetting.ASK);
        assertFalse(testElement.$.permission.disabled);
        assertFalse(testElement.$.permission.querySelector<HTMLElement>(
                                                '#ask')!.hidden);

        testElement.site = createRawSiteException(origin, {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.BLOCK,
          source: SiteSettingSource.PREFERENCE,
        });

        // In addition to the assertions below, the main goal of this test is to
        // ensure we do not hit any assertions when choosing BLOCK as a setting.
        assertEquals(testElement.$.permission.value, ContentSetting.BLOCK);
        assertFalse(testElement.$.permission.disabled);
        assertFalse(
            testElement.$.permission.querySelector<HTMLElement>(
                                        '#block')!.hidden);
      });

  test(
      'File System Write: ASK/BLOCK can be chosen as a preference by users',
      function() {
        const origin = 'https://www.example.com';
        testElement.category = ContentSettingsTypes.FILE_SYSTEM_WRITE;
        testElement.label = 'Save to original files';
        testElement.site = createRawSiteException(origin, {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.ASK,
          source: SiteSettingSource.PREFERENCE,
        });

        // In addition to the assertions below, the main goal of this test is to
        // ensure we do not hit any assertions when choosing ASK as a setting.
        assertEquals(testElement.$.permission.value, ContentSetting.ASK);
        assertFalse(testElement.$.permission.disabled);
        assertFalse(testElement.$.permission.querySelector<HTMLElement>(
                                                '#ask')!.hidden);

        testElement.site = createRawSiteException(origin, {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.BLOCK,
          source: SiteSettingSource.PREFERENCE,
        });

        // In addition to the assertions below, the main goal of this test is to
        // ensure we do not hit any assertions when choosing BLOCK as a setting.
        assertEquals(testElement.$.permission.value, ContentSetting.BLOCK);
        assertFalse(testElement.$.permission.disabled);
        assertFalse(
            testElement.$.permission.querySelector<HTMLElement>(
                                        '#block')!.hidden);
      });

  test('Chooser exceptions getChooserExceptionList API used', async function() {
    const origin = 'https://www.example.com';
    const otherOrigin = 'https://www.otherexample.com';

    const prefsUsb = createSiteSettingsPrefs(
        /*defaultsList=*/[], /*exceptionsList=*/[], /*chooserExceptionsList=*/[
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.USB_DEVICES,
              [
                createRawChooserException(
                    ChooserType.USB_DEVICES, [createRawSiteException(origin)],
                    {displayName: 'Gadget'}),
                createRawChooserException(
                    ChooserType.USB_DEVICES,
                    [createRawSiteException(
                        origin, {source: SiteSettingSource.POLICY})],
                    {displayName: 'Gizmo'}),
                createRawChooserException(
                    ChooserType.USB_DEVICES,
                    [createRawSiteException(otherOrigin)],
                    {displayName: 'Widget'}),
              ]),
        ]);
    browserProxy.setPrefs(prefsUsb);

    testElement.category = ContentSettingsTypes.USB_DEVICES;
    testElement.chooserType = ChooserType.USB_DEVICES;
    testElement.label = 'USB';
    testElement.site = createRawSiteException(origin, {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ASK,
      source: SiteSettingSource.PREFERENCE,
    });

    const chooserType =
        await browserProxy.whenCalled('getChooserExceptionList');
    assertEquals(ChooserType.USB_DEVICES, chooserType);

    // Flush the container to ensure that the container is populated.
    flush();

    // Ensure that only the chooser exceptions with the same origin are
    // rendered.
    const deviceEntries = testElement.shadowRoot!.querySelectorAll(
        'site-details-permission-device-entry');

    assertEquals(deviceEntries.length, 2);

    // The first device entry is a user granted exception.
    const firstDeviceDisplayName =
        deviceEntries[0]!.shadowRoot!.querySelector('.url-directionality');
    assertTrue(!!firstDeviceDisplayName);
    assertEquals(firstDeviceDisplayName.textContent!.trim(), 'Gadget');
    assertFalse(!!deviceEntries[0]!.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertFalse(deviceEntries[0]!.$.resetSite.hidden);

    // The second device entry is a policy granted exception.
    const secondDeviceDisplayName =
        deviceEntries[1]!.shadowRoot!.querySelector('.url-directionality');
    assertTrue(!!secondDeviceDisplayName);
    assertEquals(secondDeviceDisplayName.textContent!.trim(), 'Gizmo');
    assertTrue(!!deviceEntries[1]!.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertTrue(deviceEntries[1]!.$.resetSite.hidden);
  });

  test(
      'Chooser exceptions only incognito permission does not show device entry',
      async function() {
        const origin = 'https://www.example.com';

        const prefsUsb = createSiteSettingsPrefs(
            /*defaultsList=*/[], /*exceptionsList=*/[],
            /*chooserExceptionsList=*/[
              createContentSettingTypeToValuePair(
                  ContentSettingsTypes.USB_DEVICES,
                  [
                    createRawChooserException(
                        ChooserType.USB_DEVICES,
                        [createRawSiteException(origin, {incognito: true})],
                        {displayName: 'Gadget'}),
                  ]),
            ]);
        browserProxy.setPrefs(prefsUsb);

        testElement.category = ContentSettingsTypes.USB_DEVICES;
        testElement.chooserType = ChooserType.USB_DEVICES;
        testElement.label = 'USB';
        testElement.site = createRawSiteException(origin, {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.ASK,
          source: SiteSettingSource.PREFERENCE,
        });

        const chooserType =
            await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);

        // Flush the container to ensure that the container is populated.
        flush();

        // Ensure that no any device entry as the chooser exception only has
        // incognito permission.
        const deviceEntries = testElement.shadowRoot!.querySelectorAll(
            'site-details-permission-device-entry');

        assertEquals(deviceEntries.length, 0);
      });

  test(
      'Chooser exception origin with slash ending should still match',
      async function() {
        const origin = 'https://www.example.com';
        const originWithSlashEnding = 'https://www.example.com/';

        const prefsUsb = createSiteSettingsPrefs(
            /*defaultsList=*/[], /*exceptionsList=*/[],
            /*chooserExceptionsList=*/[
              createContentSettingTypeToValuePair(
                  ContentSettingsTypes.USB_DEVICES,
                  [
                    createRawChooserException(
                        ChooserType.USB_DEVICES,
                        [createRawSiteException(originWithSlashEnding)],
                        {displayName: 'Gadget'}),
                  ]),
            ]);
        browserProxy.setPrefs(prefsUsb);

        testElement.category = ContentSettingsTypes.USB_DEVICES;
        testElement.chooserType = ChooserType.USB_DEVICES;
        testElement.label = 'USB';
        testElement.site = createRawSiteException(origin, {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.ASK,
          source: SiteSettingSource.PREFERENCE,
        });

        const chooserType =
            await browserProxy.whenCalled('getChooserExceptionList');
        assertEquals(ChooserType.USB_DEVICES, chooserType);

        // Flush the container to ensure that the container is populated.
        flush();

        // Ensure that the chooser exception site origin with slash ending
        // still shows up.
        const deviceEntries = testElement.shadowRoot!.querySelectorAll(
            'site-details-permission-device-entry');
        assertEquals(deviceEntries.length, 1);
        assertTrue(!!deviceEntries[0]);
        const deviceDisplayName =
            deviceEntries[0].shadowRoot!.querySelector('.url-directionality');
        assertTrue(!!deviceDisplayName);
        assertEquals(deviceDisplayName.textContent!.trim(), 'Gadget');
        assertFalse(!!deviceEntries[0].shadowRoot!.querySelector(
            'cr-policy-pref-indicator'));
        assertFalse(deviceEntries[0].$.resetSite.hidden);
      });
});
