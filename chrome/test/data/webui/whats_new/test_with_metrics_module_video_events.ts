// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // A video started in this module.
  window.top.postMessage(
      {
        data: {
          event: 'video_started',
          module_name: 'ChromeFeature',
          section: 'spotlight',
          order: '1',
        },
      },
      'chrome://whats-new/');

  // A video ended in this module.
  window.top.postMessage(
      {
        data: {
          event: 'video_ended',
          module_name: 'ChromeVideoEndFeature',
          section: 'spotlight',
          order: '3',
        },
      },
      'chrome://whats-new/');

  // A video ended in this module.
  window.top.postMessage(
      {
        data: {
          event: 'play_clicked',
          module_name: 'ChromeVideoFeature',
          section: 'spotlight',
          order: '1',
        },
      },
      'chrome://whats-new/');

  // A video ended in this module.
  window.top.postMessage(
      {
        data: {
          event: 'pause_clicked',
          module_name: 'ChromeVideoFeature',
          section: 'spotlight',
          order: '2',
        },
      },
      'chrome://whats-new/');

  // A video ended in this module.
  window.top.postMessage(
      {
        data: {
          event: 'restart_clicked',
          module_name: 'ChromeVideoFeature',
          section: 'spotlight',
          order: '3',
        },
      },
      'chrome://whats-new/');
};
