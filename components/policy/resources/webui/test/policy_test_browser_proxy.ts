// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {BrowserProxy} from './../browser_proxy.js';

const policyPageMojoMigrationEnabled =
    loadTimeData.getBoolean('policyPageMojoMigrationEnabled');

/**
 * Must be kept in sync with the C++ enums of the same names (see
 * components/policy/core/common/policy_types.h).
 */
export enum PolicyScope {
  SCOPE_USER_VAL = 0,
  SCOPE_DEVICE_VAL = 1,
}

export enum PolicyLevel {
  LEVEL_RECOMMENDED_VAL = 0,
  LEVEL_MANDATORY_VAL = 1,
}

export enum PolicySource {
  SOURCE_ENTERPRISE_DEFAULT_VAL = 0,
  SOURCE_COMMAND_LINE_VAL = 1,
  SOURCE_CLOUD_VAL = 2,
  SOURCE_ACTIVE_DIRECTORY_VAL = 3,
  SOURCE_PLATFORM_VAL = 5,
  SOURCE_MERGED_VAL = 7,
  SOURCE_CLOUD_FROM_ASH_VAL = 8,
  SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE_VAL = 9,
}

export enum Presets {
  PRESET_CUSTOM,
  PRESET_CBCM,
  PRESET_LOCAL_MACHINE,
  PRESET_CLOUD_ACCOUNT,
}

export type PolicyType =
    'boolean'|'integer'|'number'|'string'|'list'|'dictionary';

export interface PolicyNamespace {
  [policyName: string]: PolicyType;
}

export interface PolicySchema {
  chrome: PolicyNamespace;
  [extensionId: string]: PolicyNamespace;
}

// Object mapping policy information types to their values.
export interface PolicyInfo {
  namespace: string;
  name: string;
  source: PolicySource;
  scope: PolicyScope;
  level: PolicyLevel;
  value: unknown;
}

// Object mapping details that need to be copied between policy rows to their
// enum values.
export interface PresetAtrributes {
  source: PolicySource;
  scope: PolicyScope;
  level: PolicyLevel;
}

export const SourceNamesToValues: {[key: string]: PolicySource} = {
  sourceEnterpriseDefault: PolicySource.SOURCE_ENTERPRISE_DEFAULT_VAL,
  commandLine: PolicySource.SOURCE_COMMAND_LINE_VAL,
  cloud: PolicySource.SOURCE_CLOUD_VAL,
  sourceActiveDirectory: PolicySource.SOURCE_ACTIVE_DIRECTORY_VAL,
  platform: PolicySource.SOURCE_PLATFORM_VAL,
  merged: PolicySource.SOURCE_MERGED_VAL,
  cloud_from_ash: PolicySource.SOURCE_CLOUD_FROM_ASH_VAL,
  restrictedManagedGuestSessionOverride:
      PolicySource.SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE_VAL,
};

export const ScopeNamesToValues: {[key: string]: PolicyScope} = {
  user: PolicyScope.SCOPE_USER_VAL,
  machine: PolicyScope.SCOPE_DEVICE_VAL,
};

export const LevelNamesToValues: {[key: string]: PolicyLevel} = {
  recommended: PolicyLevel.LEVEL_RECOMMENDED_VAL,
  mandatory: PolicyLevel.LEVEL_MANDATORY_VAL,
};

export namespace PolicyTestBrowserProxy {
  export function applyTestPolicies(
      policies: string, profileSeparationResponse: string) {
    if (policyPageMojoMigrationEnabled) {
      return BrowserProxy.getInstance().handler.setLocalTestPolicies(
          policies, profileSeparationResponse);
    } else {
      return sendWithPromise(
          'setLocalTestPolicies', policies, profileSeparationResponse);
    }
  }

  export function listenPoliciesUpdates() {
    return sendWithPromise('listenPoliciesUpdates');
  }

  export function revertTestPolicies() {
    if (policyPageMojoMigrationEnabled) {
      BrowserProxy.getInstance().handler.revertLocalTestPolicies();
    } else {
      chrome.send('revertLocalTestPolicies');
    }
  }

  export function restartWithTestPolicies(jsonString: string) {
    if (policyPageMojoMigrationEnabled) {
      BrowserProxy.getInstance().handler.restartBrowser(jsonString);
    } else {
      chrome.send('restartBrowser', [jsonString]);
    }
  }

  export function setUserAffiliation(affiliation: boolean) {
    if (policyPageMojoMigrationEnabled) {
      return BrowserProxy.getInstance().handler.setUserAffiliated(affiliation);
    } else {
      return sendWithPromise('setUserAffiliation', affiliation);
    }
  }

  export async function getAppliedTestPolicies(): Promise<PolicyInfo[]> {
    let policies: string;
    if (policyPageMojoMigrationEnabled) {
      policies =
          (await BrowserProxy.getInstance().handler.getAppliedTestPolicies())
              .policies;
    } else {
      policies = await sendWithPromise('getAppliedTestPolicies');
    }
    if (!policies.length) {
      return [];
    }
    return JSON.parse(policies);
  }
}
