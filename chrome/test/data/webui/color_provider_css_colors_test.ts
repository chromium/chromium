// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

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

  test(
      'test ui side and chrome side color provider colors added to css stylesheet',
      function(done) {
        link.href = 'chrome://theme/colors.css?sets=ui,chrome';
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
      });

  test('test fetching ui color set only', function(done) {
    link.href = 'chrome://theme/colors.css?sets=ui';
    link.onload = function() {
      const style = getComputedStyle(document.body);
      // Check that we are able to query for a ui/ side color.
      assertNotEquals('', style.getPropertyValue('--color-accent'));
      // Check that we are not able to query for a chrome/ side color.
      assertEquals(
          '',
          style.getPropertyValue('--color-app-menu-highlight-severity-low'));
      done();
    };
    link.onerror = function(e) {
      done(new Error('Error loading colors.css' + e));
    };
  });
});
