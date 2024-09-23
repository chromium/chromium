// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppManagementBrowserProxy, AppManagementComponentBrowserProxy, AppManagementToggleRowElement, CrToggleElement} from 'chrome://os-settings/os_settings.js';
import {App, PageCallbackRouter, PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeMediaDevices} from '../fake_media_devices.js';

import {FakePageHandler} from './fake_page_handler.js';
import {TestAppManagementStore} from './test_store.js';

type AppConfig = Partial<App>;

export class TestAppManagementBrowserProxy extends TestBrowserProxy implements
    AppManagementComponentBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: FakePageHandler;

  constructor(handler: FakePageHandler) {
    super(['recordEnumerationValue']);
    this.handler = handler;
    this.callbackRouter = new PageCallbackRouter();
  }

  recordEnumerationValue(metricName: string, value: number, enumSize: number) {
    this.methodCalled('recordEnumerationValue', metricName, value, enumSize);
  }
}

export let fakeComponentBrowserProxy: TestAppManagementBrowserProxy|null = null;

/**
 * Create an app for testing purpose.
 */
export function createApp(id: string, config?: AppConfig): App {
  return FakePageHandler.createApp(id, config);
}

export function setupFakeHandler(): FakePageHandler {
  const browserProxy = AppManagementBrowserProxy.getInstance();
  const fakeHandler = new FakePageHandler(
      browserProxy.callbackRouter.$.bindNewPipeAndPassRemote());
  browserProxy.handler = fakeHandler.getRemote();

  fakeComponentBrowserProxy = new TestAppManagementBrowserProxy(fakeHandler);
  AppManagementComponentBrowserProxy.setInstance(fakeComponentBrowserProxy);
  return fakeHandler;
}

/**
 * Replace the app management store instance with a new, empty
 * TestAppManagementStore.
 */
export function replaceStore(): TestAppManagementStore {
  const store = new TestAppManagementStore({selectedAppId: null});
  store.setReducersEnabled(true);
  store.replaceSingleton();
  return store;
}

export function isHidden(element: Element|null): boolean {
  return !isVisible(element);
}

/**
 * Replace the current body of the test with a new element.
 */
export function replaceBody(element: Element): void {
  window.history.replaceState({}, '', '/');
  document.body.appendChild(element);
}

export function getPermissionItemByType(
    view: Element, permissionType: PermissionTypeIndex): Element {
  const element = view.shadowRoot!.querySelector(
      '[permission-type=' + permissionType + ']');
  assertTrue(!!element);
  return element;
}

export function getPermissionToggleByType(
    view: Element,
    permissionType: PermissionTypeIndex): AppManagementToggleRowElement {
  const toggleRowElement =
      getPermissionItemByType(view, permissionType)
          .shadowRoot!.querySelector('app-management-toggle-row');
  assertTrue(!!toggleRowElement);
  return toggleRowElement;
}

export function getPermissionCrToggleByType(
    view: Element, permissionType: PermissionTypeIndex): CrToggleElement {
  const toggleElement = getPermissionToggleByType(view, permissionType)
                            .shadowRoot!.querySelector('cr-toggle');
  assertTrue(!!toggleElement);
  return toggleElement;
}

export function isHiddenByDomIf(element: HTMLElement|null): boolean {
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

export async function addFakeSensor(
    mediaDevices: FakeMediaDevices,
    permissionType: PermissionTypeIndex): Promise<void> {
  switch (PermissionType[permissionType]) {
    case PermissionType.kCamera:
      mediaDevices.addDevice('videoinput', 'Fake Camera');
      break;
    case PermissionType.kMicrophone:
      mediaDevices.addDevice('audioinput', 'Fake Microphone');
      break;
    case PermissionType.kUnknown:
    case PermissionType.kLocation:
    case PermissionType.kContacts:
    case PermissionType.kStorage:
    case PermissionType.kNotifications:
    case PermissionType.kPrinting:
    case PermissionType.kFileHandling:
      assertNotReached();
  }
  await flushTasks();
}
