// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {ContentSetting, ContentSettingsTypes, HandlerEntry, ProtocolEntry, RawChooserException, RawSiteException, RecentSitePermissions, SiteSettingSource, SiteSettingsPrefsBrowserProxy, ZoomLevelEntry} from 'chrome://settings/lazy_load.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

import {createOriginInfo, createSiteGroup,createSiteSettingsPrefs, getContentSettingsTypeFromChooserType, SiteSettingsPref} from './test_util.js';
// clang-format on


/**
 * A test version of SiteSettingsPrefsBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @implements {SiteSettingsPrefsBrowserProxy}
 */
export class TestSiteSettingsPrefsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'fetchBlockAutoplayStatus',
      'fetchZoomLevels',
      'getAllSites',
      'getChooserExceptionList',
      'getDefaultValueForContentType',
      'getFormattedBytes',
      'getExceptionList',
      'getOriginPermissions',
      'isOriginValid',
      'isPatternValidForType',
      'observeProtocolHandlers',
      'observeProtocolHandlersEnabledState',
      'removeIgnoredHandler',
      'removeProtocolHandler',
      'removeZoomLevel',
      'resetCategoryPermissionForPattern',
      'resetChooserExceptionForSite',
      'setCategoryPermissionForPattern',
      'setDefaultValueForContentType',
      'setOriginPermissions',
      'setProtocolDefault',
      'setProtocolHandlerDefault',
      'updateIncognitoStatus',
      'clearEtldPlus1DataAndCookies',
      'clearOriginDataAndCookies',
      'recordAction',
      'getCookieSettingDescription',
      'getRecentSitePermissions',
    ]);

    /** @private {boolean} */
    this.hasIncognito_ = false;

    /** @private {!SiteSettingsPref} */
    this.prefs_ = createSiteSettingsPrefs([], [], []);

    /** @private {!Array<ZoomLevelEntry>} */
    this.zoomList_ = [];

    /** @private {!Array<!ProtocolEntry>} */
    this.protocolHandlers_ = [];

    /** @private {!Array<!HandlerEntry>} */
    this.ignoredProtocols_ = [];

    /** @private {boolean} */
    this.isOriginValid_ = true;

    /** @private {boolean} */
    this.isPatternValidForType_ = true;

    /** @private {string} */
    this.cookieSettingDesciption_ = '';

    /** @private {!Array<!RecentSitePermissions>} */
    this.recentSitePermissions_ = [];
  }

  /**
   * Pretends an incognito session started or ended.
   * @param {boolean} hasIncognito True for session started.
   */
  setIncognito(hasIncognito) {
    this.hasIncognito_ = hasIncognito;
    webUIListenerCallback('onIncognitoStatusChanged', hasIncognito);
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!SiteSettingsPref} prefs The prefs to set.
   */
  setPrefs(prefs) {
    this.prefs_ = prefs;

    // Notify all listeners that their data may be out of date.
    for (const type in prefs.defaults) {
      webUIListenerCallback('contentSettingCategoryChanged', type);
    }
    for (const type in this.prefs_.exceptions) {
      const exceptionList = this.prefs_.exceptions[type];
      for (let i = 0; i < exceptionList.length; ++i) {
        webUIListenerCallback(
            'contentSettingSitePermissionChanged', type,
            exceptionList[i].origin, '');
      }
    }
    for (const type in this.prefs_.chooserExceptions) {
      const chooserExceptionList = this.prefs_.chooserExceptions[type];
      for (let i = 0; i < chooserExceptionList.length; ++i) {
        webUIListenerCallback('contentSettingChooserPermissionChanged', type);
      }
    }
  }

  /**
   * Sets one exception for a given category, replacing any existing exceptions
   * for the same origin. Note this ignores embedding origins.
   * @param {!ContentSettingsTypes} category The category the new
   *     exception belongs to.
   * @param {!RawSiteException} newException The new preference to add/replace.
   */
  setSingleException(category, newException) {
    // Remove entries from the current prefs which have the same origin.
    const newPrefs = /** @type {!Array<RawSiteException>} */
        (this.prefs_.exceptions[category].filter((categoryException) => {
          if (categoryException.origin !== newException.origin) {
            return true;
          }
        }));
    newPrefs.push(newException);
    this.prefs_.exceptions[category] = newPrefs;

    webUIListenerCallback(
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
   * @param {!Array<ProtocolEntry>} list The protocol handlers list to set.
   */
  setProtocolHandlers(list) {
    // Shallow copy of the passed-in array so mutation won't impact the source
    this.protocolHandlers_ = list.slice();
  }

  /**
   * Sets the prefs to use when testing.
   * @param {!Array<!HandlerEntry>} list
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
      const type = contentTypes[i];
      const exceptionList = this.prefs_.exceptions[type];
      for (let j = 0; j < exceptionList.length; ++j) {
        let effectiveSetting = blanketSetting;
        if (blanketSetting === ContentSetting.DEFAULT) {
          effectiveSetting = this.prefs_.defaults[type].setting;
          exceptionList[j].source = SiteSettingSource.DEFAULT;
        }
        exceptionList[j].setting = effectiveSetting;
      }
    }

    this.setPrefs(this.prefs_);
    this.methodCalled(
        'setOriginPermissions', [origin, contentTypes, blanketSetting]);
  }

  /** @override */
  getAllSites(contentTypes) {
    this.methodCalled('getAllSites', contentTypes);
    const origins_set = new Set();

    contentTypes.forEach((contentType) => {
      this.prefs_.exceptions[contentType].forEach((exception) => {
        if (exception.origin.includes('*')) {
          return;
        }
        origins_set.add(exception.origin);
      });
    });

    const origins_array = [...origins_set];
    const result = [];
    origins_array.forEach((origin, index) => {
      // Functionality to get the eTLD+1 from an origin exists only on the
      // C++ side, so just do an (incorrect) approximate extraction here.
      const host = new URL(origin).host;
      let urlParts = host.split('.');
      urlParts = urlParts.slice(Math.max(urlParts.length - 2, 0));
      const etldPlus1Name = urlParts.join('.');

      const existing = result.find(siteGroup => {
        return siteGroup.etldPlus1 === etldPlus1Name;
      });

      const mockUsage = index * 100;

      // TODO(https://crbug.com/1021606): Add test where existing evaluates to
      // true.
      if (existing) {
        const originInfo = createOriginInfo(origin, {usage: mockUsage});
        existing.origins.push(originInfo);
      } else {
        const entry = createSiteGroup(etldPlus1Name, [origin], mockUsage);
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
    const pref = this.prefs_.defaults[contentType];
    assert(pref !== undefined, 'Pref is missing for ' + contentType);
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
    assert(pref !== undefined, 'Pref is missing for ' + contentType);

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
  getChooserExceptionList(chooserType) {
    // The UI uses the |chooserType| to retrieve the prefs for a chooser
    // permission, however the test stores the permissions with the setting
    // category, so we need to get the content settings type that pertains to
    // this chooser type.
    const setting = getContentSettingsTypeFromChooserType(chooserType);
    assert(
        setting != null,
        'ContentSettingsType mapping missing for ' + chooserType);

    // Create a deep copy of the pref so that the chooser-exception-list element
    // is able update the UI appropriately when incognito mode is toggled.
    const pref = /** @type {!Array<!RawChooserException>} */ (
        JSON.parse(JSON.stringify(this.prefs_.chooserExceptions[setting])));
    assert(pref !== undefined, 'Pref is missing for ' + chooserType);

    if (this.hasIncognito_) {
      for (let i = 0; i < pref.length; ++i) {
        const incognitoElements = [];
        for (let j = 0; j < pref[i].sites.length; ++j) {
          // Skip preferences that are not controlled by policy since opening an
          // incognito session does not automatically grant permission to
          // chooser exceptions that have been granted in the main session.
          if (pref[i].sites[j].source !== SiteSettingSource.POLICY) {
            continue;
          }

          // Copy |sites[i]| to avoid changing the original |sites[i]|.
          const incognitoSite = Object.assign({}, pref[i].sites[j]);
          incognitoElements.push(
              Object.assign(incognitoSite, {incognito: true}));
        }
        pref[i].sites.push(...incognitoElements);
      }
    }

    this.methodCalled('getChooserExceptionList', chooserType);
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
  isPatternValidForType(pattern, category) {
    this.methodCalled('isPatternValidForType', [pattern, category]);
    return Promise.resolve({
      isValid: this.isPatternValidForType_,
      reason: this.isPatternValidForType_ ? '' : 'pattern is invalid',
    });
  }

  /**
   * Specify whether isPatternValidForType should succeed or fail.
   */
  setIsPatternValidForType(isValid) {
    this.isPatternValidForType_ = isValid;
  }

  /** @override */
  resetCategoryPermissionForPattern(
      primaryPattern, secondaryPattern, contentType, incognito) {
    this.methodCalled(
        'resetCategoryPermissionForPattern',
        [primaryPattern, secondaryPattern, contentType, incognito]);
  }

  /** @override */
  resetChooserExceptionForSite(
      chooserType, origin, embeddingOrigin, exception) {
    this.methodCalled(
        'resetChooserExceptionForSite',
        [chooserType, origin, embeddingOrigin, exception]);
  }

  /** @override */
  getOriginPermissions(origin, contentTypes) {
    this.methodCalled('getOriginPermissions', [origin, contentTypes]);

    const exceptionList = [];
    contentTypes.forEach(function(contentType) {
      let setting;
      let source;
      const isSet = this.prefs_.exceptions[contentType].some(originPrefs => {
        if (originPrefs.origin === origin) {
          setting = originPrefs.setting;
          source = originPrefs.source;
          return true;
        }
        return false;
      });

      if (!isSet) {
        this.prefs_.chooserExceptions[contentType].some(chooserException => {
          return chooserException.sites.some(originPrefs => {
            if (originPrefs.origin === origin) {
              setting = originPrefs.setting;
              source = originPrefs.source;
              return true;
            }
            return false;
          });
        });
      }

      assert(
          setting !== undefined,
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
  }

  /** @override */
  fetchZoomLevels() {
    webUIListenerCallback('onZoomLevelsChanged', this.zoomList_);
    this.methodCalled('fetchZoomLevels');
  }

  /** @override */
  removeZoomLevel(host) {
    this.methodCalled('removeZoomLevel', [host]);
  }

  /** @override */
  observeProtocolHandlers() {
    webUIListenerCallback('setHandlersEnabled', true);
    webUIListenerCallback('setProtocolHandlers', this.protocolHandlers_);
    webUIListenerCallback('setIgnoredProtocolHandlers', this.ignoredProtocols_);
    this.methodCalled('observeProtocolHandlers');
  }

  /** @override */
  observeProtocolHandlersEnabledState() {
    webUIListenerCallback('setHandlersEnabled', true);
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

  /** @override */
  clearEtldPlus1DataAndCookies() {
    this.methodCalled('clearEtldPlus1DataAndCookies');
  }

  /** @override */
  clearOriginDataAndCookies() {
    this.methodCalled('clearOriginDataAndCookies');
  }

  /** @override */
  recordAction() {
    this.methodCalled('recordAction');
  }

  /** @param {string} label */
  setCookieSettingDescription(label) {
    this.cookieSettingDesciption_ = label;
  }

  /** @override */
  getCookieSettingDescription() {
    this.methodCalled('getCookieSettingDescription');
    return Promise.resolve(this.cookieSettingDesciption_);
  }

  /** @param {!Array<!RecentSitePermissions>} permissions */
  setRecentSitePermissions(permissions) {
    this.recentSitePermissions_ = permissions;
  }

  /** @override */
  getRecentSitePermissions() {
    this.methodCalled('getRecentSitePermissions');
    return Promise.resolve(this.recentSitePermissions_);
  }

  /** @override */
  getDefaultCaptureDevices() {}

  /** @override */
  setDefaultCaptureDevice() {}

  /** @override */
  setProtocolHandlerDefault(value) {
    this.methodCalled('setProtocolHandlerDefault', value);
  }
}
