// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CellularInfoElement} from 'chrome://diagnostics/cellular_info.js';
import {CpuCardElement} from 'chrome://diagnostics/cpu_card.js';
import {DataPointElement} from 'chrome://diagnostics/data_point.js';
import {DiagnosticsCardElement} from 'chrome://diagnostics/diagnostics_card.js';
import {EthernetInfoElement} from 'chrome://diagnostics/ethernet_info.js';
import {PercentBarChartElement} from 'chrome://diagnostics/percent_bar_chart.js';
import {RealtimeCpuChartElement} from 'chrome://diagnostics/realtime_cpu_chart.js';
import {RoutineResultEntryElement} from 'chrome://diagnostics/routine_result_entry.js';
import {RoutineResultListElement} from 'chrome://diagnostics/routine_result_list.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {WifiInfoElement} from 'chrome://diagnostics/wifi_info.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from '../test_util.js';

/**
 * Helper function for getting a data-point element.
 * @param {?T} element
 * @param {string} selector
 * @template T
 * @return {!DataPointElement}
 */
export function getDataPoint(element, selector) {
  return /** @type {!DataPointElement} */ (
      element.shadowRoot.querySelector(`data-point${selector}`));
}

/**
 * Helper function for getting the value property from a data-point element.
 * @param {?T} element
 * @param {string} selector
 * @template T
 * @return {string}
 */
export function getDataPointValue(element, selector) {
  return `${getDataPoint(element, selector).value}`;
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
      /** @type {!CrButtonElement} */ (
          element.shadowRoot.querySelector('#runTestsButton'));
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
      /** @type {!CrButtonElement} */ (
          element.shadowRoot.querySelector('#stopTestsButton'));
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
      /** @type {!CrButtonElement} */ (
          element.shadowRoot.querySelector('#toggleReportButton'));
  assertTrue(!!button);
  return button;
}

/**
 * Helper function checks data-point visibility and content against expectation.
 * @param {?T} container
 * @param {string} selector
 * @param {string} expectedHeaderText
 * @param {string} expectedValueText
 * @template T
 * @throws {Error}
 */
export function assertDataPointHasExpectedHeaderAndValue(
    container, selector, expectedHeaderText, expectedValueText) {
  const dataPoint = getDataPoint(container, selector);
  assertTrue(isVisible(dataPoint));
  assertEquals(expectedHeaderText, dataPoint.header);
  assertEquals(expectedValueText, dataPoint.value);
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
  assertTrue(
      text.trim().indexOf(subStr) !== -1,
      `expected text "${text}" to contain "${subStr}"`);
}

/**
 * Helper function to check that a substring does not exist in an element.
 * @param {?Element} element
 * @param {string} text substring to check
 * @throws {Error}
 */
export function assertElementDoesNotContainText(element, text) {
  assertTextDoesNotContain(element.textContent, text);
}

/**
 * Helper function to check that a substring does not exist in a string.
 * @param {string} text
 * @param {string} subStr substring to check
 * @throws {Error}
 */
export function assertTextDoesNotContain(text, subStr) {
  assertTrue(
      text.trim().indexOf(subStr) === -1,
      `expected text "${text}" not to contain "${subStr}"`);
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
      /** @type {!RoutineSectionElement} */ (
          element.shadowRoot.querySelector('routine-section'));
  assertTrue(!!routineSection);
  return routineSection;
}

/**
 * Helper function for getting a wifi-info element from a
 * network-info element.
 * @param {?T} element
 * @template T
 * @return {!WifiInfoElement}
 */
export function getWifiInfoElement(element) {
  return /** @type {!WifiInfoElement} */ (
      element.shadowRoot.querySelector('wifi-info'));
}

/**
 * Helper function for getting a cellular-info element from a
 * network-info element.
 * @param {?T} element
 * @template T
 * @return {!CellularInfoElement}
 */
export function getCellularInfoElement(element) {
  return /** @type {!CellularInfoElement} */ (
      element.shadowRoot.querySelector('cellular-info'));
}

/**
 * Helper function for getting an ethernet-info element from a
 * network-info element.
 * @param {?T} element
 * @template T
 * @return {!EthernetInfoElement}
 */
export function getEthernetInfoElement(element) {
  return /** @type {!EthernetInfoElement} */ (
      element.shadowRoot.querySelector('ethernet-info'));
}

/**
 * Helper function for getting an element from a navigation-view-panel element.
 * @param {?T} element
 * @param {string} selector
 * @template T
 * @return {!HTMLElement}
 */
export function getNavigationViewPanelElement(element, selector) {
  const navPanel = element.shadowRoot.querySelector('navigation-view-panel');
  return /** @type {!HTMLElement} */ (
      navPanel.shadowRoot.querySelector(`#${selector}`));
}
