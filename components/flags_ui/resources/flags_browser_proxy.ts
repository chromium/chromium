// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * `enabled` and `is_default` are only set if the feature is single valued.
 * `enabled` is true if the feature is currently enabled.
 * `is_default` is true if the feature is in its default state.
 * `options` is only set if the entry has multiple values.
 */
export interface Feature {
  internal_name: string;
  name: string;
  description: string;
  enabled: boolean;
  is_default: boolean;
  supported_platforms: string[];
  origin_list_value?: string;
  string_value?: string;
  options?: Array<{
    internal_name: string,
    description: string,
    selected: boolean,
  }>;
  links?: string[];
}

export interface ExperimentalFeaturesData {
  supportedFeatures: Feature[];
  // <if expr="not is_ios">
  unsupportedFeatures: Feature[];
  // </if>
  needsRestart: boolean;
  showBetaChannelPromotion: boolean;
  showDevChannelPromotion: boolean;
  // <if expr="chromeos_ash">
  showOwnerWarning: boolean;
  // </if>
  // <if expr="chromeos_lacros or chromeos_ash">
  showSystemFlagsLink: boolean;
  // </if>
}

export interface FlagsBrowserProxy {
  // <if expr="not is_ios">
  restartBrowser(): void;
  requestDeprecatedFeatures(): Promise<ExperimentalFeaturesData>;
  // </if>
  // <if expr="is_chromeos">
  crosUrlFlagsRedirect(): void;
  // </if>
  resetAllFlags(): void;
  requestExperimentalFeatures(): Promise<ExperimentalFeaturesData>;
  enableExperimentalFeature(internalName: string, enable: boolean): void;
  selectExperimentalFeature(internalName: string, index: number): void;
  setOriginListFlag(internalName: string, value: string): void;
  setStringFlag(internalName: string, value: string): void;
}

export class FlagsBrowserProxyImpl implements FlagsBrowserProxy {
  // <if expr="not is_ios">
  restartBrowser() {
    chrome.send('restartBrowser');
  }

  requestDeprecatedFeatures() {
    return sendWithPromise('requestDeprecatedFeatures');
  }
  // </if>

  // <if expr="is_chromeos">
  crosUrlFlagsRedirect() {
    chrome.send('crosUrlFlagsRedirect');
  }
  // </if>

  resetAllFlags() {
    chrome.send('resetAllFlags');
  }

  requestExperimentalFeatures() {
    return sendWithPromise('requestExperimentalFeatures');
  }

  enableExperimentalFeature(internalName: string, enable: boolean) {
    chrome.send('enableExperimentalFeature', [internalName, String(enable)]);
  }

  selectExperimentalFeature(internalName: string, index: number) {
    chrome.send(
        'enableExperimentalFeature', [`${internalName}@${index}`, 'true']);
  }

  setOriginListFlag(internalName: string, value: string) {
    chrome.send('setOriginListFlag', [internalName, value]);
  }

  setStringFlag(internalName: string, value: string) {
    chrome.send('setStringFlag', [internalName, value]);
  }

  static getInstance(): FlagsBrowserProxy {
    return instance || (instance = new FlagsBrowserProxyImpl());
  }

  static setInstance(obj: FlagsBrowserProxy) {
    instance = obj;
  }
}

let instance: FlagsBrowserProxy|null = null;
