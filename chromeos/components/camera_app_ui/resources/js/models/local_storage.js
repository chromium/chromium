// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertBoolean, assertInstanceof, assertString} from '../chrome_util.js';

/**
 * @param {string} key
 * @param {*} defaultValue
 * @return {*} The value in storage or defaultValue if not found.
 */
function getHelper(key, defaultValue) {
  const rawValue = window.localStorage.getItem(key);
  if (rawValue === null) {
    return defaultValue;
  }
  return JSON.parse(rawValue);
}

/**
 * @param {string} key
 * @return {!Object} The object in storage or an empty object {} if not found.
 */
export function getObject(key) {
  return assertInstanceof(getHelper(key, {}), Object);
}

/**
 * @param {string} key
 * @return {string} The string in storage or an empty string "" if not found.
 */
export function getString(key) {
  return assertString(getHelper(key, ''));
}
/**
 * @param {string} key
 * @return {boolean} The boolean in storage or false if not found.
 */
export function getBool(key) {
  return assertBoolean(getHelper(key, false));
}

/**
 * @param {!Object<string>} items
 */
export function set(items) {
  for (const [key, val] of Object.entries(items)) {
    window.localStorage.setItem(key, JSON.stringify(val));
  }
}

/**
 * @param {(string|!Array<string>)} items
 */
export function remove(items) {
  if (typeof items === 'string') {
    items = [items];
  }
  for (const key of items) {
    window.localStorage.removeItem(key);
  }
}

/**
 * Clears all the items in the local storage.
 */
export function clear() {
  window.localStorage.clear();
}
