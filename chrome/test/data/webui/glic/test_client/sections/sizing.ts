// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getBrowser} from '../client.js';
import {$} from '../page_element_types.js';

$.enableTestSizingMode.addEventListener('click', () => {
  $.content.setAttribute('hidden', '');
  $.contentSizingTest.removeAttribute('hidden');
  updateSizingMode(true);
});

$.disableTestSizingMode.addEventListener('click', () => {
  $.content.removeAttribute('hidden');
  $.contentSizingTest.setAttribute('hidden', '');
  updateSizingMode(false);
});

$.enableDragResizeCheckbox.addEventListener('change', () => {
  getBrowser()!.enableDragResize!($.enableDragResizeCheckbox.checked);
});

$.growHeight.addEventListener('click', () => {
  const divElement = document.createElement('div');
  divElement.textContent = 'Some Text';
  for (let i = 0; i < 5; i++) {
    $.dump.appendChild(divElement.cloneNode(true));
  }
});

$.resetHeight.addEventListener('click', () => {
  $.dump.innerHTML = '';
});

async function updateSizingMode(inSizingTest: boolean) {
  if (!inSizingTest) {
    document.documentElement.classList.remove('fitWindow');
    return;
  }

  if (await getBrowser()!.shouldFitWindow!()) {
    $.fitWindow.checked = true;
    $.naturalSizing.checked = false;
    document.documentElement.classList.add('fitWindow');
  } else {
    $.fitWindow.checked = false;
    $.naturalSizing.checked = true;
    document.documentElement.classList.remove('fitWindow');
  }
}
