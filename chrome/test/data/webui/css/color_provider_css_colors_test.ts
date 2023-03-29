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
          // Check that rgb vars are not generated.
          assertEquals('', style.getPropertyValue('--color-accent-rgb'));
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
      // Check that rgb vars are not generated.
      assertEquals('', style.getPropertyValue('--color-accent-rgb'));
      done();
    };
    link.onerror = function(e) {
      done(new Error('Error loading colors.css' + e));
    };
  });

  test('test fetching color set with generate_rgb_vars=true', function(done) {
    link.href = 'chrome://theme/colors.css?sets=ui&generate_rgb_vars=true';
    link.onload = function() {
      const style = getComputedStyle(document.body);
      assertNotEquals('', style.getPropertyValue('--color-accent-rgb'));
      done();
    };
    link.onerror = function(e) {
      done(new Error('Error loading colors.css' + e));
    };
  });

  test('test fetching color set with generate_rgb_vars=false', function(done) {
    link.href = 'chrome://theme/colors.css?sets=ui&generate_rgb_vars=false';
    link.onload = function() {
      const style = getComputedStyle(document.body);
      assertEquals('', style.getPropertyValue('--color-accent-rgb'));
      done();
    };
    link.onerror = function(e) {
      done(new Error('Error loading colors.css' + e));
    };
  });

  test(
      'test fetching color set with invalid generate_rgb_vars value',
      function(done) {
        link.href = 'chrome://theme/colors.css?sets=ui&generate_rgb_vars=asdf';
        link.onload = function() {
          const style = getComputedStyle(document.body);
          // generate_rgb_vars when not provided with a explicitly truthy value
          // should default to false.
          assertEquals('', style.getPropertyValue('--color-accent-rgb'));
          done();
        };
        link.onerror = function(e) {
          done(new Error('Error loading colors.css' + e));
        };
      });
});
