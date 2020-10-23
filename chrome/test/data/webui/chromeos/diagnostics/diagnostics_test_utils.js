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

/**
 * Helper function for getting a realtime-cpu-chart element from a
 * diagnostics card.
 * @param {!HTMLElement} element
 * @return {!Array<!HTMLElement>}
 */
export function getRealtimeCpuChartElement(element) {
  return element.shadowRoot.querySelector('realtime-cpu-chart');
}

/**
 * Helper function for getting an array of routine-result-entry
 * element from a routine-result-list.
 * @param {!HTMLElement} element
 * @return {!Array<!HTMLElement>}
 */
export function getResultEntries(element) {
  return element.shadowRoot.querySelectorAll('routine-result-entry');
}

/**
 * Helper function for getting the routine-result-list from an element.
 * @param {!HTMLElement} element
 * @return {!HTMLElement}
 */
export function getResultList(element) {
  return element.shadowRoot.querySelector('routine-result-list');
}

/**
 * Helper function to check if a substring exists in an element.
 * @param {!HTMLElement} element
 * @param {string} substring to check
 * @throws {Error}
 */
export function assertElementContainsText(element, text) {
  assertTrue(element.textContent.trim().indexOf(text) !== -1);
}
