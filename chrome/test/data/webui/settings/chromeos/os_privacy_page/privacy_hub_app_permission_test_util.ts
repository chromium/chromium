// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {appPermissionHandlerMojom} from 'chrome://os-settings/os_settings.js';
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
  const metrics = new FakeMetricsPrivate();
  chrome.metricsPrivate = metrics as unknown as typeof chrome.metricsPrivate;
  flush();
  return metrics;
}
