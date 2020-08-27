// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testI18nProcess_NbspPlaceholder() {
  var h1 = document.querySelector('h1');
  var span = document.querySelector('span');

  assertFalse(document.documentElement.hasAttribute('i18n-processed'));
  assertEquals('', h1.textContent);
  assertEquals('', span.textContent);

  /* We can't check that the non-breaking space hack actually works because it
   * uses :psuedo-elements that are inaccessible to the DOM. Let's just check
   * that they're not auto-collapsed. */
  assertNotEqual(0, h1.offsetHeight);
  assertNotEqual(0, span.offsetHeight);

  h1.removeAttribute('i18n-content');
  assertEquals(0, h1.offsetHeight);

  span.removeAttribute('i18n-values');
  assertEquals(0, span.offsetHeight);
}
