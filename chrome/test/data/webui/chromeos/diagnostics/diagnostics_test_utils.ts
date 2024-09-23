// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {BatteryStatusCardElement} from 'chrome://diagnostics/battery_status_card.js';
import {CellularInfoElement} from 'chrome://diagnostics/cellular_info.js';
import {ConnectivityCardElement} from 'chrome://diagnostics/connectivity_card.js';
import {CpuCardElement} from 'chrome://diagnostics/cpu_card.js';
import {DataPointElement} from 'chrome://diagnostics/data_point.js';
import {DiagnosticsCardElement} from 'chrome://diagnostics/diagnostics_card.js';
import {EthernetInfoElement} from 'chrome://diagnostics/ethernet_info.js';
import {MemoryCardElement} from 'chrome://diagnostics/memory_card.js';
import {NetworkInfoElement} from 'chrome://diagnostics/network_info';
import {PercentBarChartElement} from 'chrome://diagnostics/percent_bar_chart.js';
import {RealtimeCpuChartElement} from 'chrome://diagnostics/realtime_cpu_chart.js';
import {RoutineResultEntryElement} from 'chrome://diagnostics/routine_result_entry.js';
import {RoutineResultListElement} from 'chrome://diagnostics/routine_result_list.js';
import {RoutineSectionElement} from 'chrome://diagnostics/routine_section.js';
import {WifiInfoElement} from 'chrome://diagnostics/wifi_info.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

/**
 * Helper function for getting a data-point element.
 */
export function getDataPoint(
    element: HTMLElement, selector: string): DataPointElement {
  assert(element);
  return strictQuery(
      `data-point${selector}`, element.shadowRoot, DataPointElement);
}

/**
 * Helper function for getting the value property from a data-point element.
 */
export function getDataPointValue(element: any, selector: string): string {
  return `${getDataPoint(element, selector).value}`;
}

/**
 * Helper function for getting a percent-bar-chart element from a
 * diagnostics card.
 */
export function getPercentBarChartElement(element: any):
    PercentBarChartElement {
  assert(element);
  const percentBarChart =
      element.shadowRoot!.querySelector(PercentBarChartElement.is);
  assert(percentBarChart);
  return percentBarChart;
}

/**
 * Helper function for getting a realtime-cpu-chart element from a
 * diagnostics card.
 */
export function getRealtimeCpuChartElement(element: CpuCardElement|
                                           null): RealtimeCpuChartElement {
  assert(element);
  const realtimeCpuChart =
      element.shadowRoot!.querySelector(RealtimeCpuChartElement.is);
  assert(realtimeCpuChart);
  return realtimeCpuChart;
}

/**
 * Helper function for getting an array of routine-result-entry
 * element from a routine-result-list.
 */
export function getResultEntries(element: RoutineResultListElement):
    NodeListOf<RoutineResultEntryElement> {
  const entries =
      element.shadowRoot!.querySelectorAll(RoutineResultEntryElement.is);
  assert(entries);
  return entries;
}

/**
 * Helper function for getting the routine-result-list from an element.
 */
export function getResultList(element: RoutineSectionElement|
                              null): RoutineResultListElement {
  assert(element);
  const routineResultList =
      element.shadowRoot!.querySelector<RoutineResultListElement>(
          RoutineResultListElement.is);
  assert(routineResultList);
  return routineResultList;
}

/**
 * Helper function for getting an array of routine-result-entry
 * element from a routine-section.
 */
export function getResultEntriesFromSection(element: RoutineSectionElement):
    NodeListOf<RoutineResultEntryElement> {
  return getResultEntries(getResultList(element));
}

/**
 * Helper function for getting the Run Tests button from a routine-section.
 */
export function getRunTestsButtonFromSection(element: RoutineSectionElement|
                                             null): CrButtonElement {
  assert(element);
  const button =
      element.shadowRoot!.querySelector<CrButtonElement>('#runTestsButton');
  assert(button);
  return button;
}

/**
 * Helper function for getting the Stop Tests button from a routine-section.
 */
export function getStopTestsButtonFromSection(element: RoutineSectionElement|
                                              null): CrButtonElement {
  assert(element);
  const button =
      element.shadowRoot!.querySelector<CrButtonElement>('#stopTestsButton');
  assert(button);
  return button;
}

/**
 * Helper function for getting the Show/Hide Tests Report button from a
 * routine-section.
 */
export function getToggleTestReportButtonFromSection(
    elementRoutineSectionElement: RoutineSectionElement|null): CrButtonElement {
  assert(elementRoutineSectionElement);
  const button =
      elementRoutineSectionElement.shadowRoot!.querySelector<CrButtonElement>(
          '#toggleReportButton');
  assert(button);
  return button;
}

/**
 * Helper function checks data-point visibility and content against expectation.
 */
export function assertDataPointHasExpectedHeaderAndValue(
    container: any, selector: string, expectedHeaderText: string,
    expectedValueText: string) {
  const dataPoint = getDataPoint(container, selector);
  assertTrue(isVisible(dataPoint));
  assertEquals(expectedHeaderText, dataPoint.header);
  assertEquals(expectedValueText, dataPoint.value);
}

/**
 * Helper function to check if a substring exists in an element.
 */
export function assertElementContainsText(element: Element|null, text: string) {
  assert(element);
  assertTextContains(element.textContent as string, text);
}

/**
 * Helper function to check if a substring exists in a string.
 */
export function assertTextContains(text: string, subStr: string) {
  assertTrue(
      text.trim().indexOf(subStr) !== -1,
      `expected text "${text}" to contain "${subStr}"`);
}

/**
 * Helper function to check that a substring does not exist in an element.
 */
export function assertElementDoesNotContainText(
    element: Element, text: string) {
  assert(element);
  assertTextDoesNotContain(element.textContent as string, text);
}

/**
 * Helper function to check that a substring does not exist in a string.
 */
export function assertTextDoesNotContain(text: string, subStr: string) {
  assertTrue(
      text.trim().indexOf(subStr) === -1,
      `expected text "${text}" not to contain "${subStr}"`);
}

/**
 * Helper function for getting the diagnostics-card from an element.
 */
export function getDiagnosticsCard(element: BatteryStatusCardElement|
                                   CpuCardElement|MemoryCardElement|
                                   null): DiagnosticsCardElement {
  assert(element);
  const diagnosticsCard =
      element.shadowRoot!.querySelector<DiagnosticsCardElement>(
          'diagnostics-card');
  assert(diagnosticsCard);
  return diagnosticsCard;
}

/**
 * Helper function for getting the routine-section from an element.
 */
export function getRoutineSection(element: MemoryCardElement|
                                  ConnectivityCardElement|
                                  null): RoutineSectionElement {
  assert(element);
  const routineSection =
      element.shadowRoot!.querySelector<RoutineSectionElement>(
          RoutineSectionElement.is);
  assert(routineSection);
  return routineSection;
}

/**
 * Helper function for getting a wifi-info element from a
 * network-info element.
 */
export function getWifiInfoElement(element: NetworkInfoElement|
                                   null): WifiInfoElement|null {
  if (!element) {
    return null;
  }
  assert(element);
  return element.shadowRoot!.querySelector<WifiInfoElement>(WifiInfoElement.is);
}

/**
 * Helper function for getting a cellular-info element from a
 * network-info element.
 */
export function getCellularInfoElement(element: NetworkInfoElement|
                                       null): CellularInfoElement|null {
  if (!element) {
    return null;
  }
  assert(element);
  return element.shadowRoot!.querySelector<CellularInfoElement>(
      CellularInfoElement.is);
}

/**
 * Helper function for getting an ethernet-info element from a
 * network-info element.
 */
export function getEthernetInfoElement(element: NetworkInfoElement|
                                       null): EthernetInfoElement|null {
  if (!element) {
    return null;
  }
  assert(element);
  return element.shadowRoot!.querySelector<EthernetInfoElement>(
      EthernetInfoElement.is);
}

/**
 * Helper function for getting an element from a navigation-view-panel element.
 */
export function getNavigationViewPanelElement(
    element: any, selector: string): HTMLElement {
  assert(element);
  const navPanel = element.shadowRoot!.querySelector('navigation-view-panel');
  assert(navPanel);
  const navElement = navPanel.shadowRoot!.querySelector(`#${selector}`);
  assert(navElement);
  return navElement;
}
