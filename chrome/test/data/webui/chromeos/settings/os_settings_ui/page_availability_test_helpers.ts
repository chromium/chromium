// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Exports common helpers for page availability browser tests.
 */

import {routesMojom} from 'chrome://os-settings/os_settings.js';

export type SectionName = keyof typeof routesMojom.Section;

interface SectionData {
  name: SectionName;
  availableBeforeRevamp: boolean;
  availableAfterRevamp: boolean;
  availableForGuest: boolean;
}

// Keep sorted by order of menu items.
// TODO(b/272139610) Update entries below as Sections get incorporated into
// their respective Revamp Section.
export const SECTION_EXPECTATIONS: SectionData[] = [
  {
    name: 'kNetwork',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kBluetooth',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kMultiDevice',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: false,
  },
  {
    name: 'kPeople',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: false,
  },
  {
    name: 'kKerberos',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kDevice',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kPersonalization',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kSearchAndAssistant',
    availableBeforeRevamp: true,
    availableAfterRevamp: false,
    availableForGuest: true,
  },
  {
    name: 'kPrivacyAndSecurity',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kApps',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kAccessibility',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kSystemPreferences',
    availableBeforeRevamp: false,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
  {
    name: 'kDateAndTime',
    availableBeforeRevamp: true,
    availableAfterRevamp: false,
    availableForGuest: true,
  },
  {
    name: 'kLanguagesAndInput',
    availableBeforeRevamp: true,
    availableAfterRevamp: false,
    availableForGuest: true,
  },
  {
    name: 'kFiles',
    availableBeforeRevamp: true,
    availableAfterRevamp: false,
    availableForGuest: false,
  },
  {
    name: 'kPrinting',
    availableBeforeRevamp: true,
    availableAfterRevamp: false,
    availableForGuest: true,
  },
  {
    name: 'kCrostini',
    availableBeforeRevamp: true,
    availableAfterRevamp: false,
    availableForGuest: true,
  },
  {
    name: 'kReset',
    availableBeforeRevamp: true,
    availableAfterRevamp: false,
    availableForGuest: false,
  },
  {
    name: 'kAboutChromeOs',
    availableBeforeRevamp: true,
    availableAfterRevamp: true,
    availableForGuest: true,
  },
];
