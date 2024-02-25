// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SafetyCheckIconStatus, SettingsSafetyCheckChildElement} from 'chrome://settings/settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

export interface SafetyCheckChildExpectation {
  page: HTMLElement;
  iconStatus: SafetyCheckIconStatus;
  label: string;
  sublabel?: string;
  buttonLabel?: string;
  buttonAriaLabel?: string;
  buttonClass?: string;
  managedIcon?: boolean;
  rowClickable?: boolean;
}

/**
 * Verify that the safety check child inside the page has been configured as
 * specified.
 */
export function assertSafetyCheckChild({
  page,
  iconStatus,
  label,
  sublabel,
  buttonLabel,
  buttonAriaLabel,
  buttonClass,
  managedIcon,
  rowClickable,
}: SafetyCheckChildExpectation) {
  const safetyCheckChild =
      page.shadowRoot!.querySelector<SettingsSafetyCheckChildElement>(
          '#safetyCheckChild')!;
  assertTrue(safetyCheckChild.iconStatus === iconStatus);
  assertTrue(safetyCheckChild.label === label);
  assertTrue(
      (!sublabel && !safetyCheckChild.subLabel) ||
      safetyCheckChild.subLabel === sublabel);
  assertTrue(!buttonLabel || safetyCheckChild.buttonLabel === buttonLabel);
  assertTrue(
      !buttonAriaLabel || safetyCheckChild.buttonAriaLabel === buttonAriaLabel);
  assertTrue(!buttonClass || safetyCheckChild.buttonClass === buttonClass);
  assertTrue(!!managedIcon === !!safetyCheckChild.managedIcon);
  assertTrue(!!rowClickable === !!safetyCheckChild.rowClickable);
}