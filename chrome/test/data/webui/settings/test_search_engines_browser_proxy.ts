// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo, SearchEnginesInteractions, ChoiceMadeLocation} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

/**
 * A test version of SearchEnginesBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
export class TestSearchEnginesBrowserProxy extends TestBrowserProxy implements
    SearchEnginesBrowserProxy {
  private searchEnginesInfo_: SearchEnginesInfo;
  private saveGuestChoice_: boolean|null;

  constructor() {
    super([
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

    this.searchEnginesInfo_ =
        {defaults: [], actives: [], others: [], extensions: []};
    this.saveGuestChoice_ = null;
  }

  setDefaultSearchEngine(
      modelIndex: number, choiceMadeLocation: ChoiceMadeLocation,
      saveGuestChoice?: boolean|null) {
    this.methodCalled(
        'setDefaultSearchEngine', modelIndex, choiceMadeLocation,
        saveGuestChoice);
  }

  setIsActiveSearchEngine(modelIndex: number, isActive: boolean) {
    this.methodCalled('setIsActiveSearchEngine', [modelIndex, isActive]);
  }

  removeSearchEngine(modelIndex: number) {
    this.methodCalled('removeSearchEngine', modelIndex);
  }

  searchEngineEditStarted(modelIndex: number) {
    this.methodCalled('searchEngineEditStarted', modelIndex);
  }

  searchEngineEditCancelled() {
    this.methodCalled('searchEngineEditCancelled');
  }

  searchEngineEditCompleted(
      searchEngine: string, keyword: string, queryUrl: string) {
    this.methodCalled(
        'searchEngineEditCompleted', [searchEngine, keyword, queryUrl]);
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
   * Sets the response to be returned by |getSearchEnginesList|.
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
        modelIndex: 0,
        name: 'Google',
        shouldConfirmDeletion: false,
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
    modelIndex: 6,
    name: 'Omnibox extension',
    shouldConfirmDeletion: false,
    url: 'chrome-extension://dummyextensionid/?q=%s',
    urlLocked: false,
  };
}
