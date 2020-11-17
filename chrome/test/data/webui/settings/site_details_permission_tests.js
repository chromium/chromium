// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting,ContentSettingsTypes,SiteSettingSource,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair,createDefaultContentSetting,createRawSiteException,createSiteSettingsPrefs} from 'chrome://test/settings/test_util.js';
// clang-format on

/** @fileoverview Suite of tests for site-details. */
suite('SiteDetailsPermission', function() {
  /**
   * A site list element created before each test.
   * @type {SiteDetailsPermission}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  /**
   * An example pref with only camera allowed.
   * @type {SiteSettingsPref}
   */
  let prefs;

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
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('site-details-permission');
    document.body.appendChild(testElement);
  });

  function validatePermissionFlipWorks(origin, expectedContentSetting) {
    browserProxy.resetResolver('setOriginPermissions');

    // Simulate permission change initiated by the user.
    testElement.$.permission.value = expectedContentSetting;
    testElement.$.permission.dispatchEvent(new CustomEvent('change'));

    return browserProxy.whenCalled('setOriginPermissions').then((args) => {
      assertEquals(origin, args[0]);
      assertDeepEquals([testElement.category], args[1]);
      assertEquals(expectedContentSetting, args[2]);
    });
  }

  test('camera category', function() {
    const origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = ContentSettingsTypes.CAMERA;
    testElement.label = 'Camera';
    testElement.site = {
      origin: origin,
      embeddingOrigin: '',
      source: SiteSettingSource.PREFERENCE,
    };

    assertFalse(testElement.$.details.hidden);

    const header = testElement.$.details.querySelector('#permissionHeader');
    assertEquals(
        'Camera', header.innerText.trim(),
        'Widget should be labelled correctly');

    // Flip the permission and validate that prefs stay in sync.
    return validatePermissionFlipWorks(origin, ContentSetting.ALLOW)
        .then(() => {
          return validatePermissionFlipWorks(origin, ContentSetting.BLOCK);
        })
        .then(() => {
          return validatePermissionFlipWorks(origin, ContentSetting.ALLOW);
        })
        .then(() => {
          return validatePermissionFlipWorks(origin, ContentSetting.DEFAULT);
        });
  });

  test('default string is correct', function() {
    const origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = ContentSettingsTypes.CAMERA;
    testElement.label = 'Camera';
    testElement.site = {
      origin: origin,
      embeddingOrigin: '',
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.PREFERENCE,
    };

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then((args) => {
          // Check getDefaultValueForContentType was called for camera category.
          assertEquals(ContentSettingsTypes.CAMERA, args);

          // The default option will always be the first in the menu.
          assertEquals(
              'Allow (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
          browserProxy.resetResolver('getDefaultValueForContentType');
          const defaultPrefs = createSiteSettingsPrefs(
              [createContentSettingTypeToValuePair(
                  ContentSettingsTypes.CAMERA,
                  createDefaultContentSetting(
                      {setting: ContentSetting.BLOCK}))],
              []);
          browserProxy.setPrefs(defaultPrefs);
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then((args) => {
          assertEquals(ContentSettingsTypes.CAMERA, args);
          assertEquals(
              'Block (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
          browserProxy.resetResolver('getDefaultValueForContentType');
          const defaultPrefs = createSiteSettingsPrefs(
              [createContentSettingTypeToValuePair(
                  ContentSettingsTypes.CAMERA, createDefaultContentSetting())],
              []);
          browserProxy.setPrefs(defaultPrefs);
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then((args) => {
          assertEquals(ContentSettingsTypes.CAMERA, args);
          assertEquals(
              'Ask (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
        });
  });

  test('info string is correct', function() {
    const origin = 'https://www.example.com';
    testElement.category = ContentSettingsTypes.CAMERA;

    // Strings that should be shown for the permission sources that don't depend
    // on the ContentSetting value.
    const permissionSourcesNoSetting = {};
    permissionSourcesNoSetting[SiteSettingSource.DEFAULT] = '';
    permissionSourcesNoSetting[SiteSettingSource.PREFERENCE] = '';
    permissionSourcesNoSetting[SiteSettingSource.EMBARGO] =
        'Automatically blocked';
    permissionSourcesNoSetting[SiteSettingSource.INSECURE_ORIGIN] =
        'Blocked to protect your privacy';
    permissionSourcesNoSetting[SiteSettingSource.KILL_SWITCH] =
        'Temporarily blocked to protect your security';

    for (const testSource in permissionSourcesNoSetting) {
      testElement.site = {
        origin: origin,
        embeddingOrigin: origin,
        setting: ContentSetting.BLOCK,
        source: testSource,
      };
      assertEquals(
          permissionSourcesNoSetting[testSource] +
              (permissionSourcesNoSetting[testSource].length === 0 ?
                   'Block (default)\nAllow\nBlock\nAsk' :
                   '\nBlock (default)\nAllow\nBlock\nAsk'),
          testElement.$.permissionItem.innerText.trim());
      assertEquals(
          permissionSourcesNoSetting[testSource] !== '',
          testElement.$.permissionItem.classList.contains('two-line'));

      if (testSource !== SiteSettingSource.DEFAULT &&
          testSource !== SiteSettingSource.PREFERENCE &&
          testSource !== SiteSettingSource.EMBARGO) {
        assertTrue(testElement.$.permission.disabled);
      } else {
        assertFalse(testElement.$.permission.disabled);
      }
    }

    // Permissions that have been set by extensions.
    const extensionSourceStrings = {};
    extensionSourceStrings[ContentSetting.ALLOW] = 'Allowed by an extension';
    extensionSourceStrings[ContentSetting.BLOCK] = 'Blocked by an extension';
    extensionSourceStrings[ContentSetting.ASK] =
        'Setting controlled by an extension';

    for (const testSetting in extensionSourceStrings) {
      testElement.site = {
        origin: origin,
        embeddingOrigin: origin,
        setting: testSetting,
        source: SiteSettingSource.EXTENSION,
      };
      assertEquals(
          extensionSourceStrings[testSetting] +
              '\nBlock (default)\nAllow\nBlock\nAsk',
          testElement.$.permissionItem.innerText.trim());
      assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
      assertTrue(testElement.$.permission.disabled);
      assertEquals(testSetting, testElement.$.permission.value);
    }

    // Permissions that have been set by enterprise policy.
    const policySourceStrings = {};
    policySourceStrings[ContentSetting.ALLOW] = 'Allowed by your administrator';
    policySourceStrings[ContentSetting.BLOCK] = 'Blocked by your administrator';
    policySourceStrings[ContentSetting.ASK] =
        'Setting controlled by your administrator';

    for (const testSetting in policySourceStrings) {
      testElement.site = {
        origin: origin,
        embeddingOrigin: origin,
        setting: testSetting,
        source: SiteSettingSource.POLICY,
      };
      assertEquals(
          policySourceStrings[testSetting] +
              '\nBlock (default)\nAllow\nBlock\nAsk',
          testElement.$.permissionItem.innerText.trim());
      assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
      assertTrue(testElement.$.permission.disabled);
      assertEquals(testSetting, testElement.$.permission.value);
    }

    // Finally, check if changing the source from a non-user-controlled setting
    // (policy) back to a user-controlled one re-enables the control.
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ASK,
      source: SiteSettingSource.DEFAULT,
    };
    assertEquals(
        'Ask (default)\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertFalse(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);
  });

  test('info string correct for drm disabled source', function() {
    const origin = 'https://www.example.com';
    testElement.category = ContentSettingsTypes.PROTECTED_CONTENT;
    testElement.$.details.hidden = false;
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.BLOCK,
      source: SiteSettingSource.DRM_DISABLED,
    };
    assertEquals(
        'To change this setting, first turn on identifiers' +
            '\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertTrue(testElement.$.permission.disabled);
  });

  test('info string correct for ads', function() {
    const origin = 'https://www.example.com';
    testElement.category = ContentSettingsTypes.ADS;
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.BLOCK,
      source: SiteSettingSource.ADS_FILTER_BLACKLIST,
    };
    assertEquals(
        'Site shows intrusive or misleading ads' +
            '\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);

    // Check the string that shows when ads is blocked but not blacklisted.
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.BLOCK,
      source: SiteSettingSource.PREFERENCE,
    };
    assertEquals(
        'Block if site shows intrusive or misleading ads' +
            '\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);

    // Ditto for default block settings.
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.BLOCK,
      source: SiteSettingSource.DEFAULT,
    };
    assertEquals(
        'Block if site shows intrusive or misleading ads' +
            '\nBlock (default)\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertFalse(testElement.$.permission.disabled);

    // Allowing ads for unblacklisted sites shows nothing.
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.PREFERENCE,
    };
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
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.ALLOWLIST,
    };
    assertEquals(
        'Allowlisted internally\nAllow\nBlock\nAsk',
        testElement.$.permissionItem.innerText.trim());
    assertTrue(testElement.$.permissionItem.classList.contains('two-line'));
    assertTrue(testElement.$.permission.disabled);
  });

  test('sound setting default string is correct', function() {
    const origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = ContentSettingsTypes.SOUND;
    testElement.label = 'Sound';
    testElement.site = {
      origin: origin,
      embeddingOrigin: '',
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.PREFERENCE,
    };

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then((args) => {
          // Check getDefaultValueForContentType was called for sound category.
          assertEquals(ContentSettingsTypes.SOUND, args);

          // The default option will always be the first in the menu.
          assertEquals(
              'Allow (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
          browserProxy.resetResolver('getDefaultValueForContentType');
          const defaultPrefs = createSiteSettingsPrefs(
              [createContentSettingTypeToValuePair(
                  ContentSettingsTypes.SOUND,
                  createDefaultContentSetting(
                      {setting: ContentSetting.BLOCK}))],
              []);
          browserProxy.setPrefs(defaultPrefs);
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then((args) => {
          assertEquals(ContentSettingsTypes.SOUND, args);
          assertEquals(
              'Mute (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
          browserProxy.resetResolver('getDefaultValueForContentType');
          testElement.useAutomaticLabel = true;
          const defaultPrefs = createSiteSettingsPrefs(
              [createContentSettingTypeToValuePair(
                  ContentSettingsTypes.SOUND,
                  createDefaultContentSetting(
                      {setting: ContentSetting.ALLOW}))],
              []);
          browserProxy.setPrefs(defaultPrefs);
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then((args) => {
          assertEquals(ContentSettingsTypes.SOUND, args);
          assertEquals(
              'Automatic (default)', testElement.$.permission.options[0].text,
              'Default setting string should match prefs');
        });
  });

  test('sound setting block string is correct', function() {
    const origin = 'https://www.example.com';
    browserProxy.setPrefs(prefs);
    testElement.category = ContentSettingsTypes.SOUND;
    testElement.label = 'Sound';
    testElement.site = {
      origin: origin,
      embeddingOrigin: '',
      setting: ContentSetting.ALLOW,
      source: SiteSettingSource.PREFERENCE,
    };

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then((args) => {
          // Check getDefaultValueForContentType was called for sound category.
          assertEquals(ContentSettingsTypes.SOUND, args);

          // The block option will always be the third in the menu.
          assertEquals(
              'Mute', testElement.$.permission.options[2].text,
              'Block setting string should match prefs');
        });
  });

  test('ASK can be chosen as a preference by users', function() {
    const origin = 'https://www.example.com';
    testElement.category = ContentSettingsTypes.USB_DEVICES;
    testElement.label = 'USB';
    testElement.site = {
      origin: origin,
      embeddingOrigin: origin,
      setting: ContentSetting.ASK,
      source: SiteSettingSource.PREFERENCE,
    };

    // In addition to the assertions below, the main goal of this test is to
    // ensure we do not hit any assertions when choosing ASK as a setting.
    assertEquals(testElement.$.permission.value, ContentSetting.ASK);
    assertFalse(testElement.$.permission.disabled);
    assertFalse(testElement.$.permission.options.ask.hidden);
  });

  test(
      'Bluetooth scanning: ASK/BLOCK can be chosen as a preference by users',
      function() {
        const origin = 'https://www.example.com';
        testElement.category = ContentSettingsTypes.BLUETOOTH_SCANNING;
        testElement.label = 'Bluetooth-scanning';
        testElement.site = {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.ASK,
          source: SiteSettingSource.PREFERENCE,
        };

        // In addition to the assertions below, the main goal of this test is to
        // ensure we do not hit any assertions when choosing ASK as a setting.
        assertEquals(testElement.$.permission.value, ContentSetting.ASK);
        assertFalse(testElement.$.permission.disabled);
        assertFalse(testElement.$.permission.options.ask.hidden);

        testElement.site = {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.BLOCK,
          source: SiteSettingSource.PREFERENCE,
        };

        // In addition to the assertions below, the main goal of this test is to
        // ensure we do not hit any assertions when choosing BLOCK as a setting.
        assertEquals(testElement.$.permission.value, ContentSetting.BLOCK);
        assertFalse(testElement.$.permission.disabled);
        assertFalse(testElement.$.permission.options.block.hidden);
      });

  test(
      'File System Write: ASK/BLOCK can be chosen as a preference by users',
      function() {
        const origin = 'https://www.example.com';
        testElement.category = ContentSettingsTypes.FILE_SYSTEM_WRITE;
        testElement.label = 'Save to original files';
        testElement.site = {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.ASK,
          source: SiteSettingSource.PREFERENCE,
        };

        // In addition to the assertions below, the main goal of this test is to
        // ensure we do not hit any assertions when choosing ASK as a setting.
        assertEquals(testElement.$.permission.value, ContentSetting.ASK);
        assertFalse(testElement.$.permission.disabled);
        assertFalse(testElement.$.permission.options.ask.hidden);

        testElement.site = {
          origin: origin,
          embeddingOrigin: origin,
          setting: ContentSetting.BLOCK,
          source: SiteSettingSource.PREFERENCE,
        };

        // In addition to the assertions below, the main goal of this test is to
        // ensure we do not hit any assertions when choosing BLOCK as a setting.
        assertEquals(testElement.$.permission.value, ContentSetting.BLOCK);
        assertFalse(testElement.$.permission.disabled);
        assertFalse(testElement.$.permission.options.block.hidden);
      });
});
