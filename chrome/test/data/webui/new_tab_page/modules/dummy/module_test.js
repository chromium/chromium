// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, dummyDescriptor} from 'chrome://new-tab-page/new_tab_page.js';

suite('NewTabPageModulesDummyModuleTest', () => {
  setup(() => {
    PolymerTest.clearBody();
  });

  test('creates module', async () => {
    // Act.
    await dummyDescriptor.initialize();
    const module = dummyDescriptor.element;
    document.body.append(module);
    module.$.tileList.render();
  });
});
