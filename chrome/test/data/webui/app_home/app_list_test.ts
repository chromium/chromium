// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://apps/app_list.js';

import {AppInfo} from 'chrome://apps/app_home.mojom-webui.js';
import {AppListElement} from 'chrome://apps/app_list.js';
import {BrowserProxy} from 'chrome://apps/browser_proxy.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestAppHomeBrowserProxy} from './test_app_home_browser_proxy.js';

interface AppList {
  appList: AppInfo[];
}

suite('AppTest', () => {
  let appListElement: AppListElement;
  let apps: AppList;
  let testProxy: TestAppHomeBrowserProxy;

  setup(async () => {
    apps = {
      appList: [
        {
          id: 'ahfgeienlihckogmohjhadlkjgocpleb',
          startUrl: {url: 'https://test.google.com/testapp1'},
          name: 'Test App 1',
          iconUrl: {
            url:
                'chrome://extension-icon/ahfgeienlihckogmohjhadlkjgocpleb/128/1',
          },
        },
        {
          id: 'ahfgeienlihckogmotestdlkjgocpleb',
          startUrl: {url: 'https://test.google.com/testapp2'},
          name: 'Test App 2',
          iconUrl: {
            url:
                'chrome://extension-icon/ahfgeienlihckogmotestdlkjgocpleb/128/1',
          },
        },
      ],
    };
    testProxy = new TestAppHomeBrowserProxy(apps);
    BrowserProxy.setInstance(testProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    appListElement = document.createElement('app-list');
    document.body.appendChild(appListElement);
    await waitAfterNextRender(appListElement);
  });

  test('app list present', () => {
    assertTrue(!!appListElement);

    assertTrue(!!appListElement.shadowRoot!.querySelector('.icon-image'));

    assertTrue(!!appListElement.shadowRoot!.querySelector('.text-container'));

    const elementLength =
        appListElement.shadowRoot!.querySelectorAll('.element-container')
            .length;
    assertEquals(apps.appList.length, elementLength);
  });
});
