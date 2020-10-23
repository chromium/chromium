// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, dummyDescriptor} from 'chrome://new-tab-page/new_tab_page.js';
import {dummyDescriptor, FooProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {isVisible} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesDummyModuleTest', () => {
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = FooProxy.getInstance();
    testProxy.handler = TestBrowserProxy.fromClass(foo.mojom.FooHandlerRemote);
    testProxy.handler.setResultFor('getData', Promise.resolve({data: []}));
  });

  test('creates module with data', async () => {
    // Act.
    const data = [
      {
        label: 'item1',
        value: 'foo',
        imageUrl: 'foo.com',
      },
      {
        label: 'item2',
        value: 'bar',
        imageUrl: 'bar.com',
      },
      {
        label: 'item3',
        value: 'baz',
        imageUrl: 'baz.com',
      },
    ];
    testProxy.handler.setResultFor('getData', Promise.resolve({data}));
    await dummyDescriptor.initialize();
    const module = dummyDescriptor.element;
    document.body.append(module);
    module.$.tileList.render();
  });
});
