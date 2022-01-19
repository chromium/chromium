// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HelpContentProviderInterface} from 'chrome://os-feedback/feedback_types.js';
import {getHelpContentProvider, setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';

import {assertEquals} from '../../chai_assert.js';

export function fakeMojoProviderTestSuite() {
  test('SettingGettingTestProvider', () => {
    // TODO(xiangdongkong): Replace with fake when built.
    let fake_provider =
        /** @type {HelpContentProviderInterface} */ (new Object());
    setHelpContentProviderForTesting(fake_provider);
    assertEquals(fake_provider, getHelpContentProvider());
  });
}