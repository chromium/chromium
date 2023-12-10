// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {fakeHelpContentList, fakeSearchRequest, fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {HelpContentList} from 'chrome://os-feedback/feedback_types.js';
import {SearchResponse} from 'chrome://os-feedback/os_feedback_ui.mojom-webui.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeHelpContentProviderTestSuite', () => {
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
  test('getHelpContents', async () => {
    provider.setFakeSearchResponse(fakeSearchResponse);

    const response = await provider.getHelpContents(fakeSearchRequest);

    assertDeepEquals(fakeHelpContentList, response.response.results);
    assertEquals(
        fakeSearchResponse.totalResults, response.response.totalResults);
  });

  /**
   * Test that the fake help content provider returns the empty list which was
   * set explicitly.
   */
  test('getHelpContentsEmpty', async () => {
    /** @type {!HelpContentList} */
    const expectedList = [];

    /** @type {!SearchResponse} */
    const emptyResponse = {
      results: expectedList,
      totalResults: 0,
    };
    provider.setFakeSearchResponse(emptyResponse);

    const response = await provider.getHelpContents(fakeSearchRequest);

    assertDeepEquals(expectedList, response.response.results);
    assertEquals(emptyResponse.totalResults, response.response.totalResults);
    assertEquals(
        mojoString16ToString(fakeSearchRequest.query), provider.lastQuery);
  });
});
