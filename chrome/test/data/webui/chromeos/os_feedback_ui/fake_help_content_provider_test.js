// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeHelpContentList} from 'chrome://os-feedback/fake_data.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {HelpContentList, HelpContentType} from 'chrome://os-feedback/feedback_types.js';

import {assertDeepEquals, assertEquals} from '../../chai_assert.js';

export function fakeHelpContentProviderTestSuite() {
  /** @type {?FakeHelpContentProvider} */
  let provider = null;

  setup(() => {
    provider = new FakeHelpContentProvider();
  });

  teardown(() => {
    provider = null;
  });

  /**
   * Test that the fake help content provider returns the non-empty list which
   * was set explicitly.
   */
  test('getHelpContents', () => {
    provider.setFakeHelpContents(fakeHelpContentList);
    return provider.getHelpContents('wifi not working', 5)
        .then((helpContentList) => {
          assertDeepEquals(fakeHelpContentList, helpContentList);
        });
  });

  /**
   * Test that the fake help content provider returns the empty list which was
   * set explicitly.
   */
  test('getHelpContentsEmpty', () => {
    /** @type {!HelpContentList} */
    const expectedList = [];
    provider.setFakeHelpContents(expectedList);
    return provider.getHelpContents('wifi not working', 5)
        .then((helpContentList) => {
          assertDeepEquals(expectedList, helpContentList);
          assertEquals('wifi not working', provider.lastQuery);
        });
  });
}
