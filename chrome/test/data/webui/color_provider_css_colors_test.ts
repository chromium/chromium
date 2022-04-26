// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotEquals} from './chai_assert.js';

suite('ColorProviderCSSColorsTest', function() {
  test(
      'test ui side and chrome side color provider colors added to css stylesheet',
      function(done) {
        const link = document.createElement('link');
        link.rel = 'stylesheet';
        link.href = 'chrome://theme/colors.css';
        link.onload = function() {
          const style = getComputedStyle(document.body);
          // Check that we are able to query for a ui/ side color.
          assertNotEquals('', style.getPropertyValue('--color-accent'));
          // Check that we are able to query for a chrome/ side color.
          assertNotEquals(
              '',
              style.getPropertyValue(
                  '--color-app-menu-highlight-severity-low'));
          done();
        };
        link.onerror = function(e) {
          done(new Error('Error loading colors.css' + e));
        };
        document.body.appendChild(link);
      });
});
