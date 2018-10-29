// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * In the real (non-test) code, this data comes from the C++ handler.
 * Only used for tests.
 * @typedef {{defaults: !Object<settings.ContentSettingsTypes,
 *                             !DefaultContentSetting>,
 *            exceptions: !Object<settings.ContentSettingsTypes,
 *                                !Array<!RawSiteException>>}}
 */
let SiteSettingsPref;

/**
 * A test version of SiteSettingsPrefsBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @implements {settings.SiteSettingsPrefsBrowserProxy}
 */
class TestSiteSettingsPrefsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'clearFlashPref',
      'fetchUsbDevices',
      'fetchZoomLevels',
      'getAllSites',
      'getFormattedBytes',
      'getDefaultValueForContentType',
      'getExceptionList',
      'getOriginPermissions',
      'isOriginValid',
      'isPatternValid',
      'observeProtocolHandlers',
      'observeProtocolHandlersEnabledState',
      'removeIgnoredHandler',
      'removeProtocolHandler',
      'removeUsbDevice',
      'removeZoomLevel',
      'resetCategoryPermissionForPattern',
      'setCategoryPermissionForPattern',
      'setDefaultValueForContentType',
      'setOriginPermissions',
      'setProtocolDefault',
      'updateIncognitoStatus',
      'fetchBlockAutoplayStatus',
    ]);

    /** @private {boolean} */
    this.hasIncognito_ = false;

    /** @private {!SiteSettingsPref} */
    this.prefs_ = test_util.createSiteSettingsPrefs([], []);

    /** @private {!Array<ZoomLevelEntry>} */
    this.zoomList_ = [];

    /** @private {!Array<!UsbDeviceEntry>} */
    this.usbDevices_ = [];

    /** @private {!Array<!ProtocolEntry>} */
    this.protocolHandlers_ = [];

    /** @private {!Array<!HandlerEntry>} */
    this.ignoredProtocols_ = [];

    /** @private {boolean} */
    this.isOriginValid_ = true;

    /** @private {boolean} */
    this.isPatternValid_ = true;
  }

  /**
   * Pretends an incognito session started or ended.
   * @param {boolean} hasIncognito True for session started.
   */
  setIncognito(hasIncognito) {
    this.hasIncognito_ = hasIncognito;
    cr.webUIListenerCallback('onIncognitoStatusChanged', hasIncognito);
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!SiteSettingsPref} prefs The prefs to set.
   */
  setPrefs(prefs) {
    this.prefs_ = prefs;

    // Notify all listeners that their data may be out of date.
    for (const type in prefs.defaults) {
      cr.webUIListenerCallback('contentSettingCategoryChanged', type);
    }
    for (const type in this.prefs_.exceptions) {
      let exceptionList = this.prefs_.exceptions[type];
      for (let i = 0; i < exceptionList.length; ++i) {
        cr.webUIListenerCallback(
            'contentSettingSitePermissionChanged', type,
            exceptionList[i].origin, '');
      }
    }
  }

  /**
   * Sets one exception for a given category, replacing any existing exceptions
   * for the same origin. Note this ignores embedding origins.
   * @param {!settings.ContentSettingsTypes} category The category the new
   *     exception belongs to.
   * @param {!RawSiteException} newException The new preference to add/replace.
   */
  setSingleException(category, newException) {
    // Remove entries from the current prefs which have the same origin.
    const newPrefs = /** @type {!Array<RawSiteException>} */
        (this.prefs_.exceptions[category].filter((categoryException) => {
          if (categoryException.origin != newException.origin)
            return true;
        }));
    newPrefs.push(newException);
    this.prefs_.exceptions[category] = newPrefs;

    cr.webUIListenerCallback(
        'contentSettingSitePermissionChanged', category, newException.origin);
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<ZoomLevelEntry>} list The zoom list to set.
   */
  setZoomList(list) {
    this.zoomList_ = list;
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<UsbDeviceEntry>} list The usb device entry list to set.
   */
  setUsbDevices(list) {
    // Shallow copy of the passed-in array so mutation won't impact the source
    this.usbDevices_ = list.slice();
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<ProtocolEntry>} list The protocol handlers list to set.
   */
  setProtocolHandlers(list) {
    // Shallow copy of the passed-in array so mutation won't impact the source
    this.protocolHandlers_ = list.slice();
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<!HandlerEntry>}
   */
  setIgnoredProtocols(list) {
    // Shallow copy of the passed-in array so mutation won't impact the source
    this.ignoredProtocols_ = list.slice();
  }

  /** @override */
  setDefaultValueForContentType(contentType, defaultValue) {
    this.methodCalled(
        'setDefaultValueForContentType', [contentType, defaultValue]);
  }

  /** @override */
  setOriginPermissions(origin, contentTypes, blanketSetting) {
    for (let i = 0; i < contentTypes.length; ++i) {
      let type = contentTypes[i];
      let exceptionList = this.prefs_.exceptions[type];
      for (let j = 0; j < exceptionList.length; ++j) {
        let effectiveSetting = blanketSetting;
        if (blanketSetting == settings.ContentSetting.DEFAULT) {
          effectiveSetting = this.prefs_.defaults[type].setting;
          exceptionList[j].source = settings.SiteSettingSource.DEFAULT;
        }
        exceptionList[j].setting = effectiveSetting;
      }
    }

    this.setPrefs(this.prefs_);
    this.methodCalled(
        'setOriginPermissions', [origin, contentTypes, blanketSetting]);
  }

  /** @override */
  clearFlashPref(origin) {
    this.methodCalled('clearFlashPref', origin);
  }

  /** @override */
  getAllSites(contentTypes) {
    this.methodCalled('getAllSites', contentTypes);
    const origins_set = new Set();

    contentTypes.forEach((contentType) => {
      this.prefs_.exceptions[contentType].forEach((exception) => {
        if (exception.origin.includes('*'))
          return;
        origins_set.add(exception.origin);
      });
    });

    const origins_array = [...origins_set];
    let result = [];
    origins_array.forEach((origin, index) => {
      // Functionality to get the eTLD+1 from an origin exists only on the
      // C++ side, so just do an (incorrect) approximate extraction here.
      const host = new URL(origin).host;
      let urlParts = host.split('.');
      urlParts = urlParts.slice(Math.max(urlParts.length - 2, 0));
      const etldPlus1Name = urlParts.join('.');

      let existing = result.find(siteGroup => {
        return siteGroup.etldPlus1 == etldPlus1Name;
      });

      if (existing) {
        existing.origins.push(test_util.createOriginInfo(origin));
      } else {
        const entry = test_util.createSiteGroup(etldPlus1Name, [origin]);
        result.push(entry);
      }
    });

    return Promise.resolve(result);
  }

  /** @override */
  getFormattedBytes(numBytes) {
    this.methodCalled('getFormattedBytes', numBytes);
    return Promise.resolve(`${numBytes} B`);
  }

  /** @override */
  getDefaultValueForContentType(contentType) {
    this.methodCalled('getDefaultValueForContentType', contentType);
    let pref = this.prefs_.defaults[contentType];
    assert(pref != undefined, 'Pref is missing for ' + contentType);
    return Promise.resolve(pref);
  }

  /** @override */
  getExceptionList(contentType) {
    // Defer |methodCalled| call so that |then| callback for the promise
    // returned from this method runs before the one for the promise returned
    // from |whenCalled| calls in tests.
    window.setTimeout(
        () => this.methodCalled('getExceptionList', contentType), 0);
    let pref = this.prefs_.exceptions[contentType];
    assert(pref != undefined, 'Pref is missing for ' + contentType);

    if (this.hasIncognito_) {
      const incognitoElements = [];
      for (let i = 0; i < pref.length; ++i) {
        // Copy |pref[i]| to avoid changing the original |pref[i]|.
        const incognitoPref = Object.assign({}, pref[i]);
        incognitoElements.push(Object.assign(incognitoPref, {incognito: true}));
      }
      pref = pref.concat(incognitoElements);
    }

    return Promise.resolve(pref);
  }

  /** @override */
  isOriginValid(origin) {
    this.methodCalled('isOriginValid', origin);
    return Promise.resolve(this.isOriginValid_);
  }

  /**
   * Specify whether isOriginValid should succeed or fail.
   */
  setIsOriginValid(isValid) {
    this.isOriginValid_ = isValid;
  }

  /** @override */
  isPatternValid(pattern) {
    this.methodCalled('isPatternValid', pattern);
    return Promise.resolve(this.isPatternValid_);
  }

  /**
   * Specify whether isPatternValid should succeed or fail.
   */
  setIsPatternValid(isValid) {
    this.isPatternValid_ = isValid;
  }

  /** @override */
  resetCategoryPermissionForPattern(
      primaryPattern, secondaryPattern, contentType, incognito) {
    this.methodCalled(
        'resetCategoryPermissionForPattern',
        [primaryPattern, secondaryPattern, contentType, incognito]);
    return Promise.resolve();
  }

  /** @override */
  getOriginPermissions(origin, contentTypes) {
    this.methodCalled('getOriginPermissions', [origin, contentTypes]);

    const exceptionList = [];
    contentTypes.forEach(function(contentType) {
      let setting;
      let source;
      this.prefs_.exceptions[contentType].some((originPrefs) => {
        if (originPrefs.origin == origin) {
          setting = originPrefs.setting;
          source = originPrefs.source;
          return true;
        }
      });
      assert(
          setting != undefined,
          'There was no exception set for origin: ' + origin +
              ' and contentType: ' + contentType);

      exceptionList.push({
        embeddingOrigin: '',
        incognito: false,
        origin: origin,
        displayName: '',
        setting: setting,
        source: source,
      });
    }, this);
    return Promise.resolve(exceptionList);
  }

  /** @override */
  setCategoryPermissionForPattern(
      primaryPattern, secondaryPattern, contentType, value, incognito) {
    this.methodCalled(
        'setCategoryPermissionForPattern',
        [primaryPattern, secondaryPattern, contentType, value, incognito]);
    return Promise.resolve();
  }

  /** @override */
  fetchZoomLevels() {
    cr.webUIListenerCallback('onZoomLevelsChanged', this.zoomList_);
    this.methodCalled('fetchZoomLevels');
  }

  /** @override */
  removeZoomLevel(host) {
    this.methodCalled('removeZoomLevel', [host]);
  }

  /** @override */
  fetchUsbDevices() {
    this.methodCalled('fetchUsbDevices');
    return Promise.resolve(this.usbDevices_);
  }

  /** @override */
  removeUsbDevice() {
    this.methodCalled('removeUsbDevice', arguments);
  }

  /** @override */
  observeProtocolHandlers() {
    cr.webUIListenerCallback('setHandlersEnabled', true);
    cr.webUIListenerCallback('setProtocolHandlers', this.protocolHandlers_);
    cr.webUIListenerCallback(
        'setIgnoredProtocolHandlers', this.ignoredProtocols_);
    this.methodCalled('observeProtocolHandlers');
  }

  /** @override */
  observeProtocolHandlersEnabledState() {
    cr.webUIListenerCallback('setHandlersEnabled', true);
    this.methodCalled('observeProtocolHandlersEnabledState');
  }

  /** @override */
  setProtocolDefault() {
    this.methodCalled('setProtocolDefault', arguments);
  }

  /** @override */
  removeProtocolHandler() {
    this.methodCalled('removeProtocolHandler', arguments);
  }

  /** @override */
  updateIncognitoStatus() {
    this.methodCalled('updateIncognitoStatus', arguments);
  }

  /** @override */
  fetchBlockAutoplayStatus() {
    this.methodCalled('fetchBlockAutoplayStatus');
  }
}
