// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://app-settings/web_app_settings.js';

import {WebAppSettingsAppElement} from 'chrome://app-settings/web_app_settings.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppType, InstallReason, OptionalBool, WindowMode} from 'chrome://resources/cr_components/app_management/types.mojom-webui.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {TestAppManagementBrowserProxy} from './test_app_management_browser_proxy.js';

suite('AppSettingsAppTest', () => {
  setup(async () => {
    const app: App = {
      id: 'test_loader.html',
      type: AppType.kWeb,
      title: 'App Title',
      description: '',
      version: '5.1',
      size: '9.0MB',
      isPinned: OptionalBool.kFalse,
      isPolicyPinned: OptionalBool.kFalse,
      installReason: InstallReason.kUser,
      permissions: {},
      hideMoreSettings: false,
      hidePinToShelf: false,
      isPreferredApp: false,
      windowMode: WindowMode.kWindow,
      resizeLocked: false,
      hideResizeLocked: true,
      supportedLinks: [],
    };

    const testProxy = new TestAppManagementBrowserProxy();
    BrowserProxy.setInstance(testProxy);

    testProxy.fakeHandler.setApp(app);

    document.body.innerHTML = '';
    const appSettingsApp: WebAppSettingsAppElement =
        document.createElement('web-app-settings-app');
    document.body.appendChild(appSettingsApp);
    await waitAfterNextRender(appSettingsApp);
  });

  test('sanity check', function() {});
});
