// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getBrowser} from '../client.js';
import {$} from '../page_element_types.js';

export async function doStressTest() {
  const results = [];
  for (let i = 0; i < 10000; i++) {
    results.push(getBrowser()!.getUserProfileInfo!().then((_info) => {
      return true;
    }));
  }
  await Promise.all(results);
}

export async function doStressTestAndRetainMemory() {
  const results = [];
  for (let i = 0; i < 10000; i++) {
    results.push(getBrowser()!.getUserProfileInfo!());
  }
  await Promise.all(results);
}

$.stressTestEngageBtn.addEventListener('click', () => {
  doStressTest();
});

$.stressTestEngageRetainBtn.addEventListener('click', () => {
  doStressTestAndRetainMemory();
});
