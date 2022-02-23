// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {HelpContentProviderInterface} from 'chrome://os-feedback/feedback_types.js';
import {getHelpContentProvider, setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

export function fakeMojoProviderTestSuite() {
  test('SettingGettingTestHelpContentProvider', () => {
    let fake_provider =
        /** @type {HelpContentProviderInterface} */ (
            new FakeHelpContentProvider());
    setHelpContentProviderForTesting(fake_provider);
    assertEquals(fake_provider, getHelpContentProvider());
  });

  test('GetDefaultHelpContentProvider', () => {
    const provider = getHelpContentProvider();
    assertTrue(!!provider);
  });
}
