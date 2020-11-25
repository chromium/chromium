// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {ChooserType, ContentSetting, ContentSettingsTypes, DefaultContentSetting, RawChooserException, RawSiteException, SiteGroup, SiteSettingSource} from 'chrome://settings/lazy_load.js';
import {Route, Router} from 'chrome://settings/settings.js';
// clang-format on


/**
 * Helper to create an object containing a ContentSettingsType key to array or
 * object value. This is a convenience function that can eventually be
 * replaced with ES6 computed properties.
 * @param {ContentSettingsTypes} contentType The ContentSettingsType
 *     to use as the key.
 * @param {Object} value The value to map to |contentType|.
 * @return {Object<ContentSettingsTypes, Object>}
 */
export function createContentSettingTypeToValuePair(contentType, value) {
  return {setting: contentType, value: value};
}

/**
 * Helper to create a mock DefaultContentSetting.
 * @param {!Object=} override An object with a subset of the properties of
 *     DefaultContentSetting. Properties defined in |override| will
 * overwrite the defaults in this function's return value.
 * @return {!DefaultContentSetting}
 */
export function createDefaultContentSetting(override) {
  if (override === undefined) {
    override = {};
  }
  return /** @type {!DefaultContentSetting} */ (Object.assign(
      {
        setting: ContentSetting.ASK,
        source: SiteSettingSource.PREFERENCE,
      },
      override));
}

/**
 * Helper to create a mock RawSiteException.
 * @param {!string} origin The origin to use for this RawSiteException.
 * @param {!Object=} override An object with a subset of the properties of
 *     RawSiteException. Properties defined in |override| will overwrite the
 *     defaults in this function's return value.
 * @return {!RawSiteException}
 */
export function createRawSiteException(origin, override) {
  if (override === undefined) {
    override = {};
  }
  return /** @type {!RawSiteException} */ (Object.assign(
      {
        embeddingOrigin: origin,
        incognito: false,
        origin: origin,
        displayName: '',
        setting: ContentSetting.ALLOW,
        source: SiteSettingSource.PREFERENCE,
      },
      override));
}

/**
 * Helper to create a mock RawChooserException.
 * @param {!ChooserType} chooserType The chooser exception type.
 * @param {Array<!RawSiteException>} sites A list of SiteExceptions
 *     corresponding to the chooser exception.
 * @param {!Object=} override An object with a subset of the properties of
 *     RawChooserException. Properties defined in |override| will overwrite
 *     the defaults in this function's return value.
 * @return {!RawChooserException}
 */
export function createRawChooserException(chooserType, sites, override) {
  return /** @type {!RawChooserException} */ (Object.assign(
      {
        chooserType: chooserType,
        displayName: '',
        object: {},
        sites: sites,
      },
      override));
}

/**
 * In the real (non-test) code, this data comes from the C++ handler.
 * Only used for tests.
 * @typedef {{
 *   defaults: !Object<ContentSettingsTypes, !DefaultContentSetting>,
 *   exceptions: !Object<ContentSettingsTypes, !Array<!RawSiteException>>,
 *   chooserExceptions: !Object<ContentSettingsTypes,
 * !Array<!RawChooserException>>
 * }}
 */
export let SiteSettingsPref;

/**
 * Helper to create a mock SiteSettingsPref.
 * @param {!Array<{setting: ContentSettingsTypes,
 *                 value: DefaultContentSetting}>} defaultsList A list of
 *     DefaultContentSettings and the content settings they apply to, which
 *     will overwrite the defaults in the SiteSettingsPref returned by this
 *     function.
 * @param {!Array<{setting: ContentSettingsTypes,
 *                 value: !Array<RawSiteException>}>} exceptionsList A list of
 *     RawSiteExceptions and the content settings they apply to, which will
 *     overwrite the exceptions in the SiteSettingsPref returned by this
 *     function.
 * @param {!Array<{setting: ContentSettingsTypes,
 *                 value: !Array<RawChooserException>}>} chooserExceptionsList
 *     A list of RawChooserExceptions and the chooser type that they apply to,
 *     which will overwrite the exceptions in the SiteSettingsPref returned by
 *     this function.
 * @return {SiteSettingsPref}
 */
export function createSiteSettingsPrefs(
    defaultsList, exceptionsList, chooserExceptionsList = []) {
  // These test defaults reflect the actual defaults assigned to each
  // ContentSettingType, but keeping these in sync shouldn't matter for tests.
  const defaults = {};
  for (const type in ContentSettingsTypes) {
    defaults[ContentSettingsTypes[type]] = createDefaultContentSetting({});
  }
  defaults[ContentSettingsTypes.COOKIES].setting = ContentSetting.ALLOW;
  defaults[ContentSettingsTypes.IMAGES].setting = ContentSetting.ALLOW;
  defaults[ContentSettingsTypes.JAVASCRIPT].setting = ContentSetting.ALLOW;
  defaults[ContentSettingsTypes.SOUND].setting = ContentSetting.ALLOW;
  defaults[ContentSettingsTypes.POPUPS].setting = ContentSetting.BLOCK;
  defaults[ContentSettingsTypes.PROTOCOL_HANDLERS].setting =
      ContentSetting.ALLOW;
  defaults[ContentSettingsTypes.BACKGROUND_SYNC].setting = ContentSetting.ALLOW;
  defaults[ContentSettingsTypes.ADS].setting = ContentSetting.BLOCK;
  defaults[ContentSettingsTypes.SENSORS].setting = ContentSetting.ALLOW;
  defaults[ContentSettingsTypes.USB_DEVICES].setting = ContentSetting.ASK;
  defaultsList.forEach((override) => {
    defaults[override.setting] = override.value;
  });

  const chooserExceptions = {};
  const exceptions = {};
  for (const type in ContentSettingsTypes) {
    chooserExceptions[ContentSettingsTypes[type]] = [];
    exceptions[ContentSettingsTypes[type]] = [];
  }
  exceptionsList.forEach(override => {
    exceptions[override.setting] = override.value;
  });
  chooserExceptionsList.forEach(override => {
    chooserExceptions[override.setting] = override.value;
  });

  return {
    chooserExceptions: chooserExceptions,
    defaults: defaults,
    exceptions: exceptions,
  };
}

/**
 * Helper to create a mock SiteGroup.
 * @param {!string} eTLDPlus1Name The eTLD+1 of all the origins provided in
 *     |originList|.
 * @param {!Array<string>} originList A list of the origins with the same
 *     eTLD+1.
 * @param {number=} mockUsage The override initial usage value for each origin
 *     in the site group.
 * @return {!SiteGroup}
 */
export function createSiteGroup(eTLDPlus1Name, originList, mockUsage) {
  if (mockUsage === undefined) {
    mockUsage = 0;
  }
  const originInfoList =
      originList.map((origin) => createOriginInfo(origin, {usage: mockUsage}));
  return {
    etldPlus1: eTLDPlus1Name,
    origins: originInfoList,
    numCookies: 0,
    hasInstalledPWA: false,
  };
}

export function createOriginInfo(origin, override) {
  if (override === undefined) {
    override = {};
  }
  return Object.assign(
      {
        origin: origin,
        engagement: 0,
        usage: 0,
        numCookies: 0,
        hasPermissionSettings: false,
      },
      override);
}

/**
 * Helper to retrieve the category of a permission from the given
 * |chooserType|.
 * @param {ChooserType} chooserType The chooser type of the
 *     permission.
 * @return {?ContentSettingsTypes}
 */
export function getContentSettingsTypeFromChooserType(chooserType) {
  switch (chooserType) {
    case ChooserType.HID_DEVICES:
      return ContentSettingsTypes.HID_DEVICES;
    case ChooserType.SERIAL_PORTS:
      return ContentSettingsTypes.SERIAL_PORTS;
    case ChooserType.USB_DEVICES:
      return ContentSettingsTypes.USB_DEVICES;
    default:
      return null;
  }
}

export function setupPopstateListener() {
  window.addEventListener('popstate', function(event) {
    // On pop state, do not push the state onto the window.history again.
    const routerInstance = Router.getInstance();
    routerInstance.setCurrentRoute(
        /** @type {!Route} */ (
            routerInstance.getRouteForPath(window.location.pathname) ||
            routerInstance.getRoutes().BASIC),
        new URLSearchParams(window.location.search), true);
  });
}
