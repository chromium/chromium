// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from '../../chai_assert.js';

/**
 * Helper function for getting an array of data-point elements from a
 * diagnostics card.
 * @param {?T} element
 * @template T
 * @return {!NodeList<!DataPointElement>}
 */
export function getDataPointElements(element) {
  return /** @type {!NodeList<!DataPointElement>} */ (
      element.shadowRoot.querySelectorAll('data-point'));
}

/**
 * Helper function for getting a percent-bar-chart element from a
 * diagnostics card.
 * @param {?T} element
 * @template T
 * @return {!PercentBarChartElement}
 */
export function getPercentBarChartElement(element) {
  return /** @type {!PercentBarChartElement} */ (
      element.shadowRoot.querySelector('percent-bar-chart'));
}

/**
 * Helper function for getting a realtime-cpu-chart element from a
 * diagnostics card.
 * @param {?CpuCardElement} element
 * @return {!RealtimeCpuChartElement}
 */
export function getRealtimeCpuChartElement(element) {
  return /** @type {!RealtimeCpuChartElement} */ (
      element.shadowRoot.querySelector('realtime-cpu-chart'));
}

/**
 * Helper function for getting an array of routine-result-entry
 * element from a routine-result-list.
 * @param {?RoutineResultListElement} element
 * @return {!NodeList<!RoutineResultEntryElement>}
 */
export function getResultEntries(element) {
  return /** @type {!NodeList<!RoutineResultEntryElement>} */ (
      element.shadowRoot.querySelectorAll('routine-result-entry'));
}

/**
 * Helper function for getting the routine-result-list from an element.
 * @param {?RoutineSectionElement} element
 * @return {!RoutineResultListElement}
 */
export function getResultList(element) {
  return /** @type {!RoutineResultListElement} */ (
      element.shadowRoot.querySelector('routine-result-list'));
}

/**
 * Helper function for getting an array of routine-result-entry
 * element from a routine-section.
 * @param {?RoutineSectionElement} element
 * @return {!NodeList<!RoutineResultEntryElement>}
 */
export function getResultEntriesFromSection(element) {
  return getResultEntries(getResultList(element));
}

/**
 * Helper function for getting the Run Tests button from a routine-section.
 * @param {?RoutineSectionElement} element
 * @return {!CrButtonElement}
 */
export function getRunTestsButtonFromSection(element) {
  const button =
      /** @type {!CrButtonElement} */ (element.$$('#runTestsButton'));
  assertTrue(!!button);
  return button;
}

/**
 * Helper function for getting the Stop Tests button from a routine-section.
 * @param {?RoutineSectionElement} element
 * @return {!CrButtonElement}
 */
export function getStopTestsButtonFromSection(element) {
  const button =
      /** @type {!CrButtonElement} */ (element.$$('#stopTestsButton'));
  assertTrue(!!button);
  return button;
}

/**
 * Helper function for getting the Show/Hide Tests Report button from a
 * routine-section.
 * @param {?RoutineSectionElement} element
 * @return {!CrButtonElement}
 */
export function getToggleTestReportButtonFromSection(element) {
  const button =
      /** @type {!CrButtonElement} */ (element.$$('#toggleReportButton'));
  assertTrue(!!button);
  return button;
}

/**
 * Helper function to check if a substring exists in an element.
 * @param {?Element} element
 * @param {string} text substring to check
 * @throws {Error}
 */
export function assertElementContainsText(element, text) {
  assertTextContains(element.textContent, text);
}

/**
 * Helper function to check if a substring exists in a string.
 * @param {string} text
 * @param {string} subStr substring to check
 * @throws {Error}
 */
export function assertTextContains(text, subStr) {
  assertTrue(text.trim().indexOf(subStr) !== -1);
}

/**
 * Helper function for getting the diagnostics-card from an element.
 * @param {?Element} element
 * @return {!DiagnosticsCardElement}
 */
export function getDiagnosticsCard(element) {
  return /** @type {!DiagnosticsCardElement} */ (
      element.shadowRoot.querySelector('diagnostics-card'));
}

/**
 * Helper function for getting the routine-section from an element.
 * @param {?Element} element
 * @return {!RoutineSectionElement}
 */
export function getRoutineSection(element) {
  const routineSection =
      /** @type {!RoutineSectionElement} */ (element.$$('routine-section'));
  assertTrue(!!routineSection);
  return routineSection;
}
