// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

// TODO(crbug.com/474657447): Add unit tests for threads rail.
suite('NewTabPageThreadsRailTest', () => {
  setup(() => {
    loadTimeData.overrideValues({
      'enableThreadsRail_': true,
    });
  });

  test('show threads rail on composebox open', async () => {});

  test('do not show threads rail on disabled flag', async () => {});

  test('open AI Mode history on show history button click', async () => {});
});
