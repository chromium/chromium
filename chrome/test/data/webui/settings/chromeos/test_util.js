// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns whether the element both exists and is visible.
 * @param {?Element} element
 * @return {boolean}
 */
export function isVisible(element) {
  // offsetWidth and offsetHeight reflect more ways that an element could be
  // hidden, compared to checking the hidden attribute directly.
  return !!element && element.getBoundingClientRect().width > 0 &&
      element.getBoundingClientRect().height > 0;
}
