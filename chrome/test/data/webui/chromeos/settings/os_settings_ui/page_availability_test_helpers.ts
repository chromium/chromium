// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Exports common helpers for page availability browser tests.
 */

import type {routesMojom} from 'chrome://os-settings/os_settings.js';

export type SectionName = keyof typeof routesMojom.Section;

interface SectionData {
  name: SectionName;
  availableForGuest: boolean;
}

// Keep sorted by order of menu items.
export const SECTION_EXPECTATIONS: SectionData[] = [
  {
    name: 'kNetwork',
    availableForGuest: true,
  },
  {
    name: 'kBluetooth',
    availableForGuest: true,
  },
  {
    name: 'kMultiDevice',
    availableForGuest: false,
  },
  {
    name: 'kPeople',
    availableForGuest: false,
  },
  {
    name: 'kKerberos',
    availableForGuest: true,
  },
  {
    name: 'kDevice',
    availableForGuest: true,
  },
  {
    name: 'kPersonalization',
    availableForGuest: true,
  },
  {
    name: 'kPrivacyAndSecurity',
    availableForGuest: true,
  },
  {
    name: 'kApps',
    availableForGuest: true,
  },
  {
    name: 'kAccessibility',
    availableForGuest: true,
  },
  {
    name: 'kSystemPreferences',
    availableForGuest: true,
  },
  {
    name: 'kAboutChromeOs',
    availableForGuest: true,
  },
];
