// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {UserEducationInternalsPageHandlerInterface, WhatsNewEditionDemoPageInfo, WhatsNewModuleDemoPageInfo} from 'chrome://user-education-internals/user_education_internals.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestUserEducationInternalsPageHandler extends TestBrowserProxy
    implements UserEducationInternalsPageHandlerInterface {
  private whatsNewModules_: WhatsNewModuleDemoPageInfo[] = [{
    displayTitle: 'Test Module',
    moduleName: 'TestModule',
    hasBrowserCommand: true,
    isFeatureEnabled: true,
    queuePosition: 1,
  }];
  private whatsNewEditions_: WhatsNewEditionDemoPageInfo[] = [{
    displayTitle: 'Test Edition',
    editionName: 'TestEdition',
    isFeatureEnabled: true,
    hasBeenUsed: true,
    versionUsed: 120,
  }];
  private clearWhatsNewErrorMessage_: string = '';

  constructor() {
    super([
      'isFeatureEngagementInitialized',
      'getTutorials',
      'startTutorial',
      'getSessionData',
      'getFeaturePromos',
      'showFeaturePromo',
      'clearFeaturePromoData',
      'getNonIphPromos',
      'clearNonIphPromoData',
      'clearSessionData',
      'forceNewSession',
      'removeGracePeriods',
      'getNewBadges',
      'getWhatsNewModules',
      'getWhatsNewEditions',
      'getNtpPromos',
      'getNtpPromoPreferences',
      'clearNewBadgeData',
      'clearWhatsNewData',
      'clearWhatsNewModuleData',
      'clearWhatsNewEditionData',
      'clearNtpPromoData',
      'clearNtpPromoPreferences',
      'launchWhatsNewStaging',
      'updateWhatsNewVersionOverride',
    ]);
  }

  setWhatsNewModules(modules: WhatsNewModuleDemoPageInfo[]) {
    this.whatsNewModules_ = modules;
  }

  setWhatsNewEditions(editions: WhatsNewEditionDemoPageInfo[]) {
    this.whatsNewEditions_ = editions;
  }

  setClearWhatsNewErrorMessage(errorMessage: string) {
    this.clearWhatsNewErrorMessage_ = errorMessage;
  }

  isFeatureEngagementInitialized() {
    this.methodCalled('isFeatureEngagementInitialized');
    return Promise.resolve({isInitialized: true});
  }

  getTutorials() {
    this.methodCalled('getTutorials');
    return Promise.resolve({tutorialInfos: []});
  }

  startTutorial(tutorialId: string) {
    this.methodCalled('startTutorial', tutorialId);
    return Promise.resolve({errorMessage: ''});
  }

  getSessionData() {
    this.methodCalled('getSessionData');
    return Promise.resolve({sessionData: []});
  }

  getFeaturePromos() {
    this.methodCalled('getFeaturePromos');
    return Promise.resolve({featurePromos: []});
  }

  showFeaturePromo(featureName: string) {
    this.methodCalled('showFeaturePromo', featureName);
    return Promise.resolve({errorMessage: ''});
  }

  clearFeaturePromoData(featureName: string) {
    this.methodCalled('clearFeaturePromoData', featureName);
    return Promise.resolve({errorMessage: ''});
  }

  getNonIphPromos() {
    this.methodCalled('getNonIphPromos');
    return Promise.resolve({nonIphPromos: []});
  }

  clearNonIphPromoData(featureName: string) {
    this.methodCalled('clearNonIphPromoData', featureName);
    return Promise.resolve({errorMessage: ''});
  }

  clearSessionData() {
    this.methodCalled('clearSessionData');
    return Promise.resolve({errorMessage: ''});
  }

  forceNewSession() {
    this.methodCalled('forceNewSession');
    return Promise.resolve({errorMessage: ''});
  }

  removeGracePeriods() {
    this.methodCalled('removeGracePeriods');
    return Promise.resolve({errorMessage: ''});
  }

  getNewBadges() {
    this.methodCalled('getNewBadges');
    return Promise.resolve({newBadges: []});
  }

  getWhatsNewModules() {
    this.methodCalled('getWhatsNewModules');
    return Promise.resolve({whatsNewModules: this.whatsNewModules_});
  }

  getWhatsNewEditions() {
    this.methodCalled('getWhatsNewEditions');
    return Promise.resolve({whatsNewEditions: this.whatsNewEditions_});
  }

  getNtpPromos() {
    this.methodCalled('getNtpPromos');
    return Promise.resolve({ntpPromos: []});
  }

  getNtpPromoPreferences() {
    this.methodCalled('getNtpPromoPreferences');
    return Promise.resolve({ntpPromoPreferences: []});
  }

  clearNewBadgeData(featureName: string) {
    this.methodCalled('clearNewBadgeData', featureName);
    return Promise.resolve({errorMessage: ''});
  }

  clearWhatsNewData() {
    this.methodCalled('clearWhatsNewData');
    return Promise.resolve({errorMessage: this.clearWhatsNewErrorMessage_});
  }

  clearWhatsNewModuleData(moduleName: string) {
    this.methodCalled('clearWhatsNewModuleData', moduleName);
    return Promise.resolve({errorMessage: this.clearWhatsNewErrorMessage_});
  }

  clearWhatsNewEditionData(editionName: string) {
    this.methodCalled('clearWhatsNewEditionData', editionName);
    return Promise.resolve({errorMessage: this.clearWhatsNewErrorMessage_});
  }

  clearNtpPromoData(id: string) {
    this.methodCalled('clearNtpPromoData', id);
    return Promise.resolve({errorMessage: ''});
  }

  clearNtpPromoPreferences() {
    this.methodCalled('clearNtpPromoPreferences');
    return Promise.resolve({errorMessage: ''});
  }

  launchWhatsNewStaging() {
    this.methodCalled('launchWhatsNewStaging');
  }

  updateWhatsNewVersionOverride(version: number) {
    this.methodCalled('updateWhatsNewVersionOverride', version);
  }
}
