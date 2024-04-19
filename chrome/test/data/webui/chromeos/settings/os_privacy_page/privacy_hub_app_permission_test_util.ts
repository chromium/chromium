// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsPrivacyHubCameraSubpage, SettingsPrivacyHubGeolocationSubpage, SettingsPrivacyHubMicrophoneSubpage} from 'chrome://os-settings/lazy_load.js';
import {appPermissionHandlerMojom, SettingsPrivacyHubSystemServiceRow} from 'chrome://os-settings/os_settings.js';
import {AppType, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

type App = appPermissionHandlerMojom.App;

export function createApp(
    id: string, name: string, permissionType: PermissionType,
    permissionValue: TriState): App {
  const app: App = {id, name, type: AppType.kWeb, permissions: {}};
  app.permissions[permissionType] = createTriStatePermission(
      permissionType, permissionValue, /*is_managed=*/ false);
  return app;
}

export function createFakeMetricsPrivate(): FakeMetricsPrivate {
  const fakeMetricsPrivate = new FakeMetricsPrivate();
  chrome.metricsPrivate = fakeMetricsPrivate;
  flush();
  return fakeMetricsPrivate;
}

export function getSystemServicesFromSubpage(
    subpage: SettingsPrivacyHubCameraSubpage|
    SettingsPrivacyHubMicrophoneSubpage|SettingsPrivacyHubGeolocationSubpage):
    NodeListOf<SettingsPrivacyHubSystemServiceRow> {
  return subpage.shadowRoot!.querySelectorAll(
      'settings-privacy-hub-system-service-row');
}

export function getSystemServicePermissionText(
    systemService: SettingsPrivacyHubSystemServiceRow): string {
  return systemService.shadowRoot!
      .querySelector<HTMLDivElement>('#permissionState')!.innerText.trim();
}

export function getSystemServiceName(
    systemService: SettingsPrivacyHubSystemServiceRow): string {
  return systemService.shadowRoot!.querySelector<HTMLElement>(
                                      '#serviceName')!.innerText.trim();
}
