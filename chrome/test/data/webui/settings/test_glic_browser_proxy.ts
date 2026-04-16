// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserProxy, LoginPermission} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export enum Shortcut {
  MAIN = 'main',
  FOCUS_TOGGLE = 'focusToggle',
}

export class TestGlicBrowserProxy extends TestBrowserProxy implements
    GlicBrowserProxy {
  private glicShortcutResponse_: string = '';
  private glicFocusToggleShortcutResponse_: string = '';
  private glicDisallowedByAdmin_: boolean = false;
  private webActuationToggleVisibilityResponse_: boolean = false;
  private actorLoginPermissions_: LoginPermission[] = [];
  private revokeActorLoginPermissionResponse_: boolean = true;

  constructor() {
    super([
      'getActorLoginPermissions',
      'revokeActorLoginPermission',
      'setGlicOsLauncherEnabled',
      'getGlicShortcut',
      'setGlicShortcut',
      'getGlicFocusToggleShortcut',
      'setGlicFocusToggleShortcut',
      'setShortcutSuspensionState',
      'getDisallowedByAdmin',
      'getWebActuationToggleVisibility',
      'getGlicSelectionShortcut',
      'setGlicSelectionShortcut',
    ]);
  }

  override reset() {
    super.reset();
    this.glicShortcutResponse_ = '';
    this.glicFocusToggleShortcutResponse_ = '';
    this.webActuationToggleVisibilityResponse_ = false;
    this.actorLoginPermissions_ = [];
    this.revokeActorLoginPermissionResponse_ = true;
  }

  setGlicOsLauncherEnabled(enabled: boolean) {
    this.methodCalled('setGlicOsLauncherEnabled', enabled);
  }

  setGlicShortcutResponse(response: string) {
    this.glicShortcutResponse_ = response;
  }

  setGlicFocusToggleShortcutResponse(response: string) {
    this.glicFocusToggleShortcutResponse_ = response;
  }

  setShortcutResponse(shortcut: Shortcut, response: string) {
    switch (shortcut) {
      case Shortcut.MAIN:
        this.setGlicShortcutResponse(response);
        break;
      case Shortcut.FOCUS_TOGGLE:
        this.setGlicFocusToggleShortcutResponse(response);
        break;
      default:
        break;
    }
  }

  getGlicShortcut() {
    this.methodCalled('getGlicShortcut');
    return Promise.resolve(this.glicShortcutResponse_);
  }

  setGlicShortcut(shortcut: string) {
    this.methodCalled('setGlicShortcut', shortcut);
    return Promise.resolve();
  }

  getGlicFocusToggleShortcut() {
    this.methodCalled('getGlicFocusToggleShortcut');
    return Promise.resolve(this.glicFocusToggleShortcutResponse_);
  }

  setGlicFocusToggleShortcut(shortcut: string) {
    this.methodCalled('setGlicFocusToggleShortcut', shortcut);
    return Promise.resolve();
  }

  setShortcutSuspensionState(shouldSuspend: boolean) {
    this.methodCalled('setShortcutSuspensionState', shouldSuspend);
  }

  getDisallowedByAdmin() {
    this.methodCalled('getDisallowedByAdmin');
    return Promise.resolve(this.glicDisallowedByAdmin_);
  }

  setDisallowedByAdmin(disallowed: boolean) {
    this.glicDisallowedByAdmin_ = disallowed;
  }

  getWebActuationToggleVisibility() {
    this.methodCalled('getWebActuationToggleVisibility');
    return Promise.resolve(this.webActuationToggleVisibilityResponse_);
  }

  setWebActuationToggleVisibilityResponse(visible: boolean) {
    this.webActuationToggleVisibilityResponse_ = visible;
  }
  getActorLoginPermissions() {
    this.methodCalled('getActorLoginPermissions');
    return Promise.resolve(this.actorLoginPermissions_);
  }

  setActorLoginPermissions(permissions: LoginPermission[]) {
    this.actorLoginPermissions_ = permissions;
  }

  setRevokeActorLoginPermissionResponse(success: boolean) {
    this.revokeActorLoginPermissionResponse_ = success;
  }

  revokeActorLoginPermission(signonRealm: string) {
    this.methodCalled('revokeActorLoginPermission', signonRealm);
    return Promise.resolve(this.revokeActorLoginPermissionResponse_);
  }

  getGlicSelectionShortcut() {
    this.methodCalled('getGlicSelectionShortcut');
    return Promise.resolve(this.glicShortcutResponse_);
  }

  setGlicSelectionShortcut(shortcut: string) {
    this.methodCalled('setGlicSelectionShortcut', shortcut);
    return Promise.resolve();
  }
}
