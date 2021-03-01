// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {FakeSettingsPrivatePref} from 'chrome://test/settings/fake_settings_private.m.js';
// clang-format on

/**
 * @type {Array<{pref: settings.FakeSettingsPrivatePref,
 *               nextValues: Array<*>}>}
 * Test cases containing preference data. Each test case has a pref with an
 * initial value, and two "next" values used to change the pref. Intentionally,
 * for a given pref, not every "next" value is different from the previous
 * value; this tests what happens when stale changes are reported.
 */
/* #export */ const prefsTestCases = [
  {
    pref: {
      key: 'top_level_pref',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    nextValues: [false, true],
  },
  {
    pref: {
      key: 'browser.enable_flash',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    nextValues: [true, false],
  },
  {
    pref: {
      key: 'browser.enable_html5',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    nextValues: [false, false],
  },
  {
    pref: {
      key: 'device.overclock',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 0,
    },
    nextValues: [.2, .6],
  },
  {
    pref: {
      key: 'browser.on.startup.homepage',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'example.com',
    },
    nextValues: ['chromium.org', 'chrome.example.com'],
  },
  {
    pref: {
      key: 'profile.name',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'Puppy',
    },
    nextValues: ['Puppy', 'Horsey'],
  },
  {
    pref: {
      key: 'content.sites',
      type: chrome.settingsPrivate.PrefType.LIST,
      // Array of dictionaries.
      value: [
        {
          javascript: ['chromium.org', 'example.com'],
          cookies: ['example.net'],
          mic: ['example.com'],
          flash: []
        },
        {some: 4, other: 8, dictionary: 16}
      ],
    },
    nextValues: [
      [
        {
          javascript: ['example.com', 'example.net'],
          cookies: ['example.net', 'example.com'],
          mic: ['example.com']
        },
        {some: 4, other: 8, dictionary: 16}
      ],
      [
        {
          javascript: ['chromium.org', 'example.com'],
          cookies: ['chromium.org', 'example.net', 'example.com'],
          flash: ['localhost'],
          mic: ['example.com']
        },
        {some: 2.2, dictionary: 4.4}
      ],
    ],
  },
  {
    pref: {
      key: 'content_settings.exceptions.notifications',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {
        'https:\/\/foo.com,*': {
          last_used: 1442486000.4000,
          'setting': 0,
        },
        'https:\/\/bar.com,*': {
          'last_used': 1442487000.3000,
          'setting': 1,
        },
        'https:\/\/baz.com,*': {
          'last_used': 1442482000.8000,
          'setting': 2,
        },
      },
    },
    nextValues: [
      {
        'https:\/\/foo.com,*': {
          last_used: 1442486000.4000,
          'setting': 0,
        },
        'https:\/\/example.com,*': {
          'last_used': 1442489000.1000,
          'setting': 2,
        },
        'https:\/\/baz.com,*': {
          'last_used': 1442484000.9000,
          'setting': 1,
        },
      },
      {
        'https:\/\/foo.com,*': {
          last_used: 1442488000.8000,
          'setting': 1,
        },
        'https:\/\/example.com,*': {
          'last_used': 1442489000.1000,
          'setting': 2,
        },
      }
    ],
  }
];
