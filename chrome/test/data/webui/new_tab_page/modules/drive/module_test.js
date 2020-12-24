// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {driveDescriptor} from 'chrome://new-tab-page/new_tab_page.js';

suite('NewTabPageModulesDriveModuleTest', () => {
  test('module appears on render', async () => {
    await driveDescriptor.initialize();
    const module = driveDescriptor.element;
    assertTrue(!!module);
  });
});