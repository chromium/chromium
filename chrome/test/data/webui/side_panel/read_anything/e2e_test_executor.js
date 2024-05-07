// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
(() => {
  const assert = (value) => {
    if (!value) {
      console.error(`Assertion Failed: value was = ${
          value === '' ? '<empty string>' : value}`);
    }
  };

  function prune(elem) {
    if (elem.attributes.getNamedItem('lang')) {
      elem.attributes.removeNamedItem('lang');
    }
    if (elem.attributes.getNamedItem('dir')) {
      elem.attributes.removeNamedItem('dir');
    }
    for (const child of elem.children) {
      prune(child);
    }
  }

  const container =
      document.querySelector('read-anything-app')?.querySelector('#container');
  assert(container);
  prune(container);

  return container.innerHTML;
})();
