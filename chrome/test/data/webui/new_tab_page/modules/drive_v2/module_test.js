// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {driveV2Descriptor} from 'chrome://new-tab-page/new_tab_page.js';

suite('NewTabPageModulesDriveModuleTest', () => {
  setup(() => {
    PolymerTest.clearBody();
  });

  test('module appears on render', async () => {
    const module = await driveV2Descriptor.initialize();
    document.body.append(module);

    assertTrue(!!module);
  });
});
