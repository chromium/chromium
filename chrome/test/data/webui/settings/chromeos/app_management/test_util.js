// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementBrowserProxy, AppManagementComponentBrowserProxy} from 'chrome://os-settings/chromeos/os_settings.js';
import {TestAppManagementStore} from './test_store.js';
import {FakePageHandler} from './fake_page_handler.js';

/**
 * Create an app for testing purpose.
 * @param {string} id
 * @param {Object=} optConfig
 * @return {!App}
 */
export function createApp(id, config) {
  return FakePageHandler.createApp(id, config);
}

/**
 * @return {FakePageHandler}
 */
export function setupFakeHandler() {
  const browserProxy = AppManagementBrowserProxy.getInstance();
  const fakeHandler = new FakePageHandler(
      browserProxy.callbackRouter.$.bindNewPipeAndPassRemote());
  browserProxy.handler = fakeHandler.getRemote();

  const componentBrowserProxy =
      AppManagementComponentBrowserProxy.getInstance();
  componentBrowserProxy.handler = fakeHandler;
  return fakeHandler;
}

/**
 * Replace the app management store instance with a new, empty
 * TestAppManagementStore.
 * @return {TestAppManagementStore}
 */
export function replaceStore() {
  const store = new TestAppManagementStore();
  store.setReducersEnabled(true);
  store.replaceSingleton();
  return store;
}

/**
 * @param {Element} element
 * @return {bool}
 */
export function isHidden(element) {
  const rect = element.getBoundingClientRect();
  return rect.height === 0 && rect.width === 0;
}

/**
 * Replace the current body of the test with a new element.
 * @param {Element} element
 */
export function replaceBody(element) {
  PolymerTest.clearBody();

  window.history.replaceState({}, '', '/');

  document.body.appendChild(element);
}

/** @return {String} */
function getCurrentUrlSuffix() {
  return window.location.href.slice(window.location.origin.length);
}

/** @param {String} route  */
async function navigateTo(route) {
  window.history.replaceState({}, '', route);
  window.dispatchEvent(new CustomEvent('location-changed'));
  await flushTasks();
}

/**
 * @param {Element} element
 * @param {Object} permissionType
 * @return {Element}
 */
export function getPermissionItemByType(view, permissionType) {
  return view.root.querySelector('[permission-type=' + permissionType + ']');
}

/**
 * @param {Element} element
 * @param {Object} permissionType
 * @return {Element}
 */
export function getPermissionToggleByType(view, permissionType) {
  return getPermissionItemByType(view, permissionType)
      .shadowRoot.querySelector('app-management-toggle-row');
}

/**
 * @param {Element} element
 * @param {Object} permissionType
 * @return {Element}
 */
export function getPermissionCrToggleByType(view, permissionType) {
  return getPermissionToggleByType(view, permissionType)
      .shadowRoot.querySelector('cr-toggle');
}

/**
 * @param {Element} element
 * @return {boolean}
 */
export function isHiddenByDomIf(element) {
  // Happens when the dom-if is false and the element is not rendered.
  if (!element) {
    return true;
  }
  // Happens when the dom-if was showing the element and has hidden the element
  // after a state change
  if (element.style.display === 'none') {
    return true;
  }
  // The element is rendered and display !== 'none'
  return false;
}
