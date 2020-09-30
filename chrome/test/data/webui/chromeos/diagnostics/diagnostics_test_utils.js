// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper function for getting an array of data-point elements from a
 * diagnostics card.
 * @param {!HTMLElement} element
 * @return {!Array<!HTMLElement>}
 */
export function getDataPointElements(element) {
  return element.shadowRoot.querySelectorAll('data-point');
}

/**
 * Helper function for getting a percent-bar-chart element from a
 * diagnostics card.
 * @param {!HTMLElement} element
 * @return {!Array<!HTMLElement>}
 */
export function getPercentBarChartElement(element) {
  return element.shadowRoot.querySelector('percent-bar-chart');
}