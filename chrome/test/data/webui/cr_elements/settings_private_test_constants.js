// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define chrome.settingsPrivate enums, normally provided by chrome WebUI.
// NOTE: These need to be kept in sync with settings_private.idl.

chrome.settingsPrivate = chrome.settingsPrivate || {};

/** @enum {string} */
chrome.settingsPrivate.ControlledBy = {
  DEVICE_POLICY: 'DEVICE_POLICY',
  USER_POLICY: 'USER_POLICY',
  OWNER: 'OWNER',
  PRIMARY_USER: 'PRIMARY_USER',
  EXTENSION: 'EXTENSION',
};

/** @enum {string} */
chrome.settingsPrivate.Enforcement = {
  ENFORCED: 'ENFORCED',
  RECOMMENDED: 'RECOMMENDED',
  PARENT_SUPERVISED: 'PARENT_SUPERVISED',
};

/** @enum {string} */
chrome.settingsPrivate.PrefType = {
  BOOLEAN: 'BOOLEAN',
  NUMBER: 'NUMBER',
  STRING: 'STRING',
  URL: 'URL',
  LIST: 'LIST',
  DICTIONARY: 'DICTIONARY',
};
