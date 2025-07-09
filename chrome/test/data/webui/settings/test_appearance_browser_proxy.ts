// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AppearanceBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAppearanceBrowserProxy extends TestBrowserProxy implements
    AppearanceBrowserProxy {
  private defaultZoom_: number = 1;
  private isChildAccount_: boolean = false;
  private isHomeUrlValid_: boolean = true;
  private pinnedToolbarActionsAreDefaultResponse_: boolean = true;

  constructor() {
    super([
      'getDefaultZoom',
      'getThemeInfo',
      'isChildAccount',
      'openCustomizeChrome',
      'openCustomizeChromeToolbarSection',
      'recordHoverCardImagesEnabledChanged',
      'resetPinnedToolbarActions',
      'useDefaultTheme',
      // <if expr="is_linux">
      'useGtkTheme',
      'useQtTheme',
      // </if>
      'validateStartupPage',
      'pinnedToolbarActionsAreDefault',
    ]);
  }

  getDefaultZoom() {
    this.methodCalled('getDefaultZoom');
    return Promise.resolve(this.defaultZoom_);
  }

  getThemeInfo(themeId: string) {
    this.methodCalled('getThemeInfo', themeId);
    return Promise.resolve({
      id: '',
      name: 'Sports car red',
      shortName: '',
      description: '',
      version: '',
      mayDisable: false,
      enabled: false,
      isApp: false,
      offlineEnabled: false,
      optionsUrl: '',
      permissions: [],
      hostPermissions: [],
    });
  }

  isChildAccount() {
    this.methodCalled('isChildAccount');
    return this.isChildAccount_;
  }

  openCustomizeChrome() {
    this.methodCalled('openCustomizeChrome');
  }

  openCustomizeChromeToolbarSection() {
    this.methodCalled('openCustomizeChromeToolbarSection');
  }

  recordHoverCardImagesEnabledChanged(enabled: boolean) {
    this.methodCalled('recordHoverCardImagesEnabledChanged', enabled);
  }

  resetPinnedToolbarActions() {
    this.methodCalled('resetPinnedToolbarActions');
  }

  useDefaultTheme() {
    this.methodCalled('useDefaultTheme');
  }

  // <if expr="is_linux">
  useGtkTheme() {
    this.methodCalled('useGtkTheme');
  }

  useQtTheme() {
    this.methodCalled('useQtTheme');
  }
  // </if>

  setDefaultZoom(defaultZoom: number) {
    this.defaultZoom_ = defaultZoom;
  }

  setIsChildAccount(isChildAccount: boolean) {
    this.isChildAccount_ = isChildAccount;
  }

  validateStartupPage(url: string) {
    this.methodCalled('validateStartupPage', url);
    return Promise.resolve(this.isHomeUrlValid_);
  }

  setValidStartupPageResponse(isValid: boolean) {
    this.isHomeUrlValid_ = isValid;
  }

  pinnedToolbarActionsAreDefault() {
    this.methodCalled('pinnedToolbarActionsAreDefault');
    return Promise.resolve(this.pinnedToolbarActionsAreDefaultResponse_);
  }

  setPinnedToolbarActionsAreDefaultResponse(areDefault: boolean) {
    this.pinnedToolbarActionsAreDefaultResponse_ = areDefault;
  }
}
