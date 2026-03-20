// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo, CategorizedTemplateUrls, SearchEnginesInteractions, ChoiceMadeLocation} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

/**
 * A test version of SearchEnginesBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
export class TestSearchEnginesBrowserProxy extends TestBrowserProxy implements
    SearchEnginesBrowserProxy {
  private categorizedTemplateUrls_: CategorizedTemplateUrls = {
    activeSiteShortcuts: [],
    inactiveSiteShortcuts: [],
    activeFeatureShortcuts: [],
    inactiveFeatureShortcuts: [],
  };

  private searchEnginesInfo_: SearchEnginesInfo =
      {defaults: [], actives: [], others: [], extensions: []};

  private saveGuestChoice_: boolean|null = null;

  constructor() {
    super([
      'getCategorizedTemplateUrls',
      'getSearchEnginesList',
      'getSaveGuestChoice',
      'removeSearchEngine',
      'searchEngineEditCancelled',
      'searchEngineEditCompleted',
      'searchEngineEditStarted',
      'setDefaultSearchEngine',
      'setIsActiveSearchEngine',
      'validateSearchEngineInput',
      'recordSearchEnginesPageHistogram',
    ]);
  }

  setDefaultSearchEngine(
      id: number, choiceMadeLocation: ChoiceMadeLocation,
      saveGuestChoice?: boolean|null) {
    this.methodCalled(
        'setDefaultSearchEngine', id, choiceMadeLocation, saveGuestChoice);
  }

  setIsActiveSearchEngine(id: number, isActive: boolean) {
    this.methodCalled('setIsActiveSearchEngine', [id, isActive]);
  }

  removeSearchEngine(id: number) {
    this.methodCalled('removeSearchEngine', id);
  }

  searchEngineEditStarted(id: number) {
    this.methodCalled('searchEngineEditStarted', id);
  }

  searchEngineEditCancelled() {
    this.methodCalled('searchEngineEditCancelled');
  }

  searchEngineEditCompleted(
      searchEngine: string, keyword: string, queryUrl: string) {
    this.methodCalled(
        'searchEngineEditCompleted', [searchEngine, keyword, queryUrl]);
  }

  getCategorizedTemplateUrls() {
    this.methodCalled('getCategorizedTemplateUrls');
    return Promise.resolve(this.categorizedTemplateUrls_);
  }

  getSearchEnginesList() {
    this.methodCalled('getSearchEnginesList');
    return Promise.resolve(this.searchEnginesInfo_);
  }

  getSaveGuestChoice() {
    this.methodCalled('getSaveGuestChoice');
    return Promise.resolve(this.saveGuestChoice_);
  }

  validateSearchEngineInput(fieldName: string, fieldValue: string) {
    this.methodCalled('validateSearchEngineInput', [fieldName, fieldValue]);
    return Promise.resolve(true);
  }

  recordSearchEnginesPageHistogram(interaction: SearchEnginesInteractions) {
    this.methodCalled('recordSearchEnginesPageHistogram', interaction);
  }

  /**
   * Sets the response to be returned by `getCategorizedTemplateUrls`.
   */
  setCategorizedTemplateUrls(categorizedTemplateUrls: CategorizedTemplateUrls) {
    this.categorizedTemplateUrls_ = categorizedTemplateUrls;
  }

  /**
   * Sets the response to be returned by `getSearchEnginesList`.
   */
  setSearchEnginesInfo(searchEnginesInfo: SearchEnginesInfo) {
    this.searchEnginesInfo_ = searchEnginesInfo;
  }

  /**
   * Sets whether the DSE choice should be persisted for guest profiles.
   * Null if the checkbox is not available.
   */
  setSaveGuestChoice(saveGuestChoice: boolean|null) {
    this.saveGuestChoice_ = saveGuestChoice;
  }
}

export function createSampleSearchEngine(override?: Partial<SearchEngine>):
    SearchEngine {
  return Object.assign(
      {
        canBeDefault: false,
        canBeEdited: false,
        canBeRemoved: false,
        canBeActivated: false,
        canBeDeactivated: false,
        default: false,
        displayName: 'Google',
        // TODO(b/317357143): Rename to `isManaged` when the UI for DSP and SS
        //                    are unified.
        iconURL: 'http://www.google.com/favicon.ico',
        iconPath: 'images/foo.png',
        id: 0,
        isManaged: false,
        isOmniboxExtension: false,
        isPrepopulated: false,
        isStarterPack: false,
        keyword: 'google.com',
        name: 'Google',
        shouldConfirmRemoval: false,
        url: 'https://search.foo.com/search?p=%s',
        urlLocked: false,
      },
      override || {});
}

export function createSampleOmniboxExtension(): SearchEngine {
  return {
    canBeDefault: false,
    canBeEdited: false,
    canBeRemoved: false,
    canBeActivated: false,
    canBeDeactivated: false,
    default: false,
    displayName: 'Omnibox extension displayName',
    iconPath: 'images/foo.png',
    extension: {
      icon: 'chrome://extension-icon/some-extension-icon',
      id: 'dummyextensionid',
      name: 'Omnibox extension',
      canBeDisabled: false,
    },
    id: 0,
    isManaged: false,
    isOmniboxExtension: true,
    isPrepopulated: false,
    isStarterPack: false,
    keyword: 'oe',
    name: 'Omnibox extension',
    shouldConfirmRemoval: false,
    url: 'chrome-extension://dummyextensionid/?q=%s',
    urlLocked: false,
  };
}
