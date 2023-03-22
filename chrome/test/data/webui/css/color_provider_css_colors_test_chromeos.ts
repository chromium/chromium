// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotEquals} from 'chrome://webui-test/chai_assert.js';

suite('ColorProviderCSSColorsTest', function() {
  let link: HTMLLinkElement;
  setup(() => {
    link = document.createElement('link');
    link.rel = 'stylesheet';
    document.body.appendChild(link);
  });

  teardown(() => {
    document.body.removeChild(link);
  });

  test('test fetching chromeos color sets', function(done) {
    link.href = 'chrome://theme/colors.css?sets=ref,sys,legacy';
    link.onload = function() {
      const style = getComputedStyle(document.body);
      // Check that we are able to query for a cros.ref color.
      assertNotEquals('', style.getPropertyValue('--cros-ref-primary100'));
      // Check that we are able to query for a cros.sys color.
      assertNotEquals('', style.getPropertyValue('--cros-sys-primary'));
      // Check that we are able to query for a legacy semantic color.
      assertNotEquals('', style.getPropertyValue('--cros-color-primary'));
      done();
    };
    link.onerror = function(e) {
      done(new Error('Error loading colors.css' + e));
    };
  });
});
