// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function renovation_wikipedia() {
  // Get list of elements to expand.
  const elems =
      document.querySelectorAll('div.collapsible-block,h2.collapsible-heading');

  // Apply 'open-block' class to elements. This makes the sections'
  // content visible.
  for (let i = 0; i < elems.length; ++i) {
    // If a block was already expanded, re-adding the 'open-block'
    // class will do nothing; no need to check if it's there already.
    elems.item(i).className += ' open-block';
  }

  // Now we force the page to load images inside the expanded
  // sections. Wikipedia article images have a lazy image placeholder
  // as well as a noscript element with an img tag (for if scripts are
  // disabled). We get the list of these elements in order. For every
  // lazy image placeholder, there is always a corresponding noscript
  // element.
  const placeholders = document.querySelectorAll(
      '.image > span.lazy-image-placeholder, ' +
      '.mwe-math-element > span.lazy-image-placeholder');
  const noscripts = document.querySelectorAll(
      '.image > noscript, .mwe-math-element > noscript');

  // Next we delete all the placeholders, then move the img elements
  // out of the noscripts, deleting the noscript element in the
  // process.
  for (let i = 0; i < placeholders.length; ++i) {
    placeholders.item(i).remove();
    const innerText = noscripts.item(i).innerText;
    noscripts.item(i).outerHTML = innerText;
  }
}

const mapRenovations = {
  'wikipedia': renovation_wikipedia,
};

function run_renovations(flist) {
  for (const funcName of flist) {
    mapRenovations[funcName]();
  }
}
