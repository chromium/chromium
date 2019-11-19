// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('test_util', function() {

  /**
   * Helper to create an object containing a ContentSettingsType key to array or
   * object value. This is a convenience function that can eventually be
   * replaced with ES6 computed properties.
   * @param {settings.ContentSettingsTypes} contentType The ContentSettingsType
   *     to use as the key.
   * @param {Object} value The value to map to |contentType|.
   * @return {Object<setting: settings.ContentSettingsTypes, value: Object>}
   */
  function createContentSettingTypeToValuePair(contentType, value) {
    return {setting: contentType, value: value};
  }

  /**
   * Helper to create a mock DefaultContentSetting.
   * @param {!Object=} override An object with a subset of the properties of
   *     DefaultContentSetting. Properties defined in |override| will
   * overwrite the defaults in this function's return value.
   * @return {DefaultContentSetting}
   */
  function createDefaultContentSetting(override) {
    if (override === undefined) {
      override = {};
    }
    return Object.assign(
        {
          setting: settings.ContentSetting.ASK,
          source: settings.SiteSettingSource.PREFERENCE,
        },
        override);
  }

  /**
   * Helper to create a mock RawSiteException.
   * @param {!string} origin The origin to use for this RawSiteException.
   * @param {!Object=} override An object with a subset of the properties of
   *     RawSiteException. Properties defined in |override| will overwrite the
   *     defaults in this function's return value.
   * @return {RawSiteException}
   */
  function createRawSiteException(origin, override) {
    if (override === undefined) {
      override = {};
    }
    return Object.assign(
        {
          embeddingOrigin: origin,
          incognito: false,
          origin: origin,
          displayName: '',
          setting: settings.ContentSetting.ALLOW,
          source: settings.SiteSettingSource.PREFERENCE,
        },
        override);
  }

  /**
   * Helper to create a mock RawChooserException.
   * @param {!settings.ChooserType} chooserType The chooser exception type.
   * @param {Array<!RawSiteException>} sites A list of SiteExceptions
   *     corresponding to the chooser exception.
   * @param {!Object=} override An object with a subset of the properties of
   *     RawChooserException. Properties defined in |override| will overwrite
   *     the defaults in this function's return value.
   * @return {RawChooserException}
   */
  function createRawChooserException(chooserType, sites, override) {
    return Object.assign(
        {chooserType: chooserType, displayName: '', object: {}, sites: sites},
        override || {});
  }

  /**
   * Helper to create a mock SiteSettingsPref.
   * @param {!Array<{setting: settings.ContentSettingsTypes,
   *                 value: DefaultContentSetting}>} defaultsList A list of
   *     DefaultContentSettings and the content settings they apply to, which
   *     will overwrite the defaults in the SiteSettingsPref returned by this
   *     function.
   * @param {!Array<{setting: settings.ContentSettingsTypes,
   *                 value: !Array<RawSiteException>}>} exceptionsList A list of
   *     RawSiteExceptions and the content settings they apply to, which will
   *     overwrite the exceptions in the SiteSettingsPref returned by this
   *     function.
   * @param {!Array<{setting: settings.ContentSettingsTypes,
   *                 value: !Array<RawChooserException>}>} chooserExceptionsList
   *     A list of RawChooserExceptions and the chooser type that they apply to,
   *     which will overwrite the exceptions in the SiteSettingsPref returned by
   *     this function.
   * @return {SiteSettingsPref}
   */
  function createSiteSettingsPrefs(
      defaultsList, exceptionsList, chooserExceptionsList = []) {
    // These test defaults reflect the actual defaults assigned to each
    // ContentSettingType, but keeping these in sync shouldn't matter for tests.
    const defaults = {};
    for (const type in settings.ContentSettingsTypes) {
      defaults[settings.ContentSettingsTypes[type]] =
          createDefaultContentSetting({});
    }
    defaults[settings.ContentSettingsTypes.COOKIES].setting =
        settings.ContentSetting.ALLOW;
    defaults[settings.ContentSettingsTypes.IMAGES].setting =
        settings.ContentSetting.ALLOW;
    defaults[settings.ContentSettingsTypes.JAVASCRIPT].setting =
        settings.ContentSetting.ALLOW;
    defaults[settings.ContentSettingsTypes.SOUND].setting =
        settings.ContentSetting.ALLOW;
    defaults[settings.ContentSettingsTypes.POPUPS].setting =
        settings.ContentSetting.BLOCK;
    defaults[settings.ContentSettingsTypes.PROTOCOL_HANDLERS].setting =
        settings.ContentSetting.ALLOW;
    defaults[settings.ContentSettingsTypes.BACKGROUND_SYNC].setting =
        settings.ContentSetting.ALLOW;
    defaults[settings.ContentSettingsTypes.ADS].setting =
        settings.ContentSetting.BLOCK;
    defaults[settings.ContentSettingsTypes.SENSORS].setting =
        settings.ContentSetting.ALLOW;
    defaults[settings.ContentSettingsTypes.USB_DEVICES].setting =
        settings.ContentSetting.ASK;
    defaultsList.forEach((override) => {
      defaults[override.setting] = override.value;
    });

    const chooserExceptions = {};
    const exceptions = {};
    for (const type in settings.ContentSettingsTypes) {
      chooserExceptions[settings.ContentSettingsTypes[type]] = [];
      exceptions[settings.ContentSettingsTypes[type]] = [];
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
   * @return {SiteGroup}
   */
  function createSiteGroup(eTLDPlus1Name, originList) {
    const originInfoList = originList.map(origin => createOriginInfo(origin));
    return {
      etldPlus1: eTLDPlus1Name,
      origins: originInfoList,
      numCookies: 0,
    };
  }

  function createOriginInfo(origin, override) {
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
   * @param {settings.ChooserType} chooserType The chooser type of the
   *     permission.
   * @return {?settings.ContentSettingsType}
   */
  function getContentSettingsTypeFromChooserType(chooserType) {
    switch (chooserType) {
      case settings.ChooserType.USB_DEVICES:
        return settings.ContentSettingsTypes.USB_DEVICES;
      default:
        return null;
    }
  }

  return {
    createContentSettingTypeToValuePair: createContentSettingTypeToValuePair,
    createDefaultContentSetting: createDefaultContentSetting,
    createOriginInfo: createOriginInfo,
    createRawChooserException: createRawChooserException,
    createRawSiteException: createRawSiteException,
    createSiteGroup: createSiteGroup,
    createSiteSettingsPrefs: createSiteSettingsPrefs,
    getContentSettingsTypeFromChooserType:
        getContentSettingsTypeFromChooserType,
  };
});
