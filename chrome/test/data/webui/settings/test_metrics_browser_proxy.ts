// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DeleteBrowsingDataAction, MetricsBrowserProxy, PrivacyElementInteractions, PrivacyGuideInteractions, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached, SafeBrowsingInteractions, SafetyCheckInteractions, SafetyCheckNotificationsModuleInteractions, SafetyCheckUnusedSitePermissionsModuleInteractions, SafetyHubCardState, SafetyHubEntryPoint, SafetyHubModuleType, SafetyHubSurfaces} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestMetricsBrowserProxy extends TestBrowserProxy implements
    MetricsBrowserProxy {
  constructor() {
    super([
      'recordAction',
      'recordSafetyCheckInteractionHistogram',
      'recordSafetyCheckNotificationsListCountHistogram',
      'recordSafetyCheckNotificationsModuleInteractionsHistogram',
      'recordSafetyCheckNotificationsModuleEntryPointShown',
      'recordSafetyCheckUnusedSitePermissionsListCountHistogram',
      'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram',
      'recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown',
      'recordSettingsPageHistogram',
      'recordPrivacyGuideFlowLengthHistogram',
      'recordSafeBrowsingInteractionHistogram',
      'recordPrivacyGuideNextNavigationHistogram',
      'recordPrivacyGuideEntryExitHistogram',
      'recordPrivacyGuideSettingsStatesHistogram',
      'recordPrivacyGuideStepsEligibleAndReachedHistogram',
      'recordDeleteBrowsingDataAction',
      'recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram',
      'recordSafetyHubCardStateClicked',
      'recordSafetyHubDashboardAnyWarning',
      'recordSafetyHubEntryPointClicked',
      'recordSafetyHubEntryPointShown',
      'recordSafetyHubImpression',
      'recordSafetyHubInteraction',
      'recordSafetyHubModuleWarningImpression',
      'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram',
      'recordSafetyHubNotificationPermissionsModuleListCountHistogram',
      'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram',
      'recordSafetyHubUnusedSitePermissionsModuleListCountHistogram',
      // <if expr="_google_chrome and is_win">
      'recordFeatureNotificationsChange',
      // </if>
    ]);
  }

  recordAction(action: string) {
    this.methodCalled('recordAction', action);
  }

  recordSafetyCheckInteractionHistogram(interaction: SafetyCheckInteractions) {
    this.methodCalled('recordSafetyCheckInteractionHistogram', interaction);
  }

  recordSafetyCheckNotificationsListCountHistogram(suggestions: number) {
    this.methodCalled(
        'recordSafetyCheckNotificationsListCountHistogram', suggestions);
  }

  recordSafetyCheckNotificationsModuleInteractionsHistogram(
      interaction: SafetyCheckNotificationsModuleInteractions) {
    this.methodCalled(
        'recordSafetyCheckNotificationsModuleInteractionsHistogram',
        interaction);
  }

  recordSafetyCheckNotificationsModuleEntryPointShown(visible: boolean) {
    this.methodCalled(
        'recordSafetyCheckNotificationsModuleEntryPointShown', visible);
  }

  recordSafetyCheckUnusedSitePermissionsListCountHistogram(suggestions:
                                                               number) {
    this.methodCalled(
        'recordSafetyCheckUnusedSitePermissionsListCountHistogram',
        suggestions);
  }

  recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    this.methodCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram',
        interaction);
  }

  recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown(visible:
                                                                  boolean) {
    this.methodCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown', visible);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    this.methodCalled('recordSettingsPageHistogram', interaction);
  }

  recordSafeBrowsingInteractionHistogram(interaction:
                                             SafeBrowsingInteractions) {
    this.methodCalled('recordSafeBrowsingInteractionHistogram', interaction);
  }

  recordPrivacyGuideNextNavigationHistogram(interaction:
                                                PrivacyGuideInteractions) {
    this.methodCalled('recordPrivacyGuideNextNavigationHistogram', interaction);
  }

  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions) {
    this.methodCalled('recordPrivacyGuideEntryExitHistogram', interaction);
  }

  recordPrivacyGuideSettingsStatesHistogram(state: PrivacyGuideSettingsStates) {
    this.methodCalled('recordPrivacyGuideSettingsStatesHistogram', state);
  }

  recordPrivacyGuideFlowLengthHistogram(steps: number) {
    this.methodCalled('recordPrivacyGuideFlowLengthHistogram', steps);
  }

  recordPrivacyGuideStepsEligibleAndReachedHistogram(
      status: PrivacyGuideStepsEligibleAndReached) {
    this.methodCalled(
        'recordPrivacyGuideStepsEligibleAndReachedHistogram', status);
  }

  recordDeleteBrowsingDataAction(action: DeleteBrowsingDataAction) {
    this.methodCalled('recordDeleteBrowsingDataAction', action);
  }

  recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    this.methodCalled(
        'recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram',
        interaction);
  }

  recordSafetyHubCardStateClicked(
      histogramName: string, state: SafetyHubCardState) {
    this.methodCalled(
        'recordSafetyHubCardStateClicked', [histogramName, state]);
  }

  recordSafetyHubDashboardAnyWarning(visible: boolean) {
    this.methodCalled('recordSafetyHubDashboardAnyWarning', visible);
  }

  recordSafetyHubEntryPointClicked(page: SafetyHubEntryPoint) {
    this.methodCalled('recordSafetyHubEntryPointClicked', page);
  }

  recordSafetyHubEntryPointShown(page: SafetyHubEntryPoint) {
    this.methodCalled('recordSafetyHubModuleWarningImpression', page);
  }

  recordSafetyHubImpression(surface: SafetyHubSurfaces) {
    this.methodCalled('recordSafetyHubImpression', surface);
  }

  recordSafetyHubInteraction(surface: SafetyHubSurfaces) {
    this.methodCalled('recordSafetyHubInteraction', surface);
  }

  recordSafetyHubModuleWarningImpression(module: SafetyHubModuleType) {
    this.methodCalled('recordSafetyHubModuleWarningImpression', module);
  }

  recordSafetyHubNotificationPermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckNotificationsModuleInteractions) {
    this.methodCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram',
        interaction);
  }

  recordSafetyHubNotificationPermissionsModuleListCountHistogram(suggestions:
                                                                     number) {
    this.methodCalled(
        'recordSafetyHubNotificationPermissionsModuleListCountHistogram',
        suggestions);
  }

  recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram(
      interaction: SafetyCheckUnusedSitePermissionsModuleInteractions) {
    this.methodCalled(
        'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram',
        interaction);
  }

  recordSafetyHubUnusedSitePermissionsModuleListCountHistogram(suggestions:
                                                                   number) {
    this.methodCalled(
        'recordSafetyHubUnusedSitePermissionsModuleListCountHistogram',
        suggestions);
  }

  // <if expr="_google_chrome and is_win">
  recordFeatureNotificationsChange(enabled: boolean) {
    this.methodCalled('recordFeatureNotificationsChange', enabled);
  }
  // </if>
}
