// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-later.top-chrome/read_anything/app.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {ReadAnythingElement} from 'chrome://read-later.top-chrome/read_anything/app.js';

suite('ReadAnythingAppTest', () => {
  let readAnythingApp: ReadAnythingElement;

  setup(async () => {
    document.body.innerHTML = '';
    readAnythingApp = document.createElement('read-anything-app');
    document.body.appendChild(readAnythingApp);
  });

  // TODO(abigailbklein): Implement tests.
  test('onFontNameChange', async () => {});

  test('showContent paragraph', async () => {});

  test('showContent heading', async () => {});

  test('showContent heading badInput', async () => {});

  test('showContent link', async () => {});

  test('showContent link badInput', async () => {});

  test('showContent staticText', async () => {});

  test('showContent staticText badInput', async () => {});

  test('showContent clearContainer', async () => {});
});
