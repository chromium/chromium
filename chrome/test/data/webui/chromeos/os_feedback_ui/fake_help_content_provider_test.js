// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeHelpContentList, fakeSearchRequest, fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {HelpContentList, mojoString16ToString, SearchResponse} from 'chrome://os-feedback/feedback_types.js';

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
    provider.setFakeSearchResponse(fakeSearchResponse);

    return provider.getHelpContents(fakeSearchRequest).then((response) => {
      assertDeepEquals(fakeHelpContentList, response.response.results);
      assertEquals(
          fakeSearchResponse.totalResults, response.response.totalResults);
    });
  });

  /**
   * Test that the fake help content provider returns the empty list which was
   * set explicitly.
   */
  test('getHelpContentsEmpty', () => {
    /** @type {!HelpContentList} */
    const expectedList = [];

    /** @type {!SearchResponse} */
    const emptyResponse = {
      results: expectedList,
      totalResults: 0,
    };

    provider.setFakeSearchResponse(emptyResponse);
    return provider.getHelpContents(fakeSearchRequest).then((response) => {
      assertDeepEquals(expectedList, response.response.results);
      assertEquals(emptyResponse.totalResults, response.response.totalResults);
      assertEquals(
          mojoString16ToString(fakeSearchRequest.query), provider.lastQuery);
    });
  });
}
