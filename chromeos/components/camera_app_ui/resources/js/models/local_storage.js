// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {(string|!Array<string>|!Object)} keys
 * @return {!Promise<!Object>}
 */
export async function get(keys) {
  let result = {};
  let sanitizedKeys = [];
  if (typeof keys === 'string') {
    sanitizedKeys = [keys];
  } else if (Array.isArray(keys)) {
    sanitizedKeys = keys;
  } else if (keys !== null && typeof keys === 'object') {
    sanitizedKeys = Object.keys(keys);

    // If any key does not exist, use the default value specified in the
    // input.
    result = Object.assign({}, keys);
  } else {
    throw new Error('WebUI localStorageGet() cannot be run with ' + keys);
  }

  for (const key of sanitizedKeys) {
    const value = window.localStorage.getItem(key);
    if (value !== null) {
      result[key] = JSON.parse(value);
    } else if (result[key] === undefined) {
      // For key that does not exist and does not have a default value, set it
      // to null.
      result[key] = null;
    }
  }
  return result;
}

/**
 * @param {!Object<string>} items
 * @return {!Promise}
 */
export async function set(items) {
  for (const [key, val] of Object.entries(items)) {
    window.localStorage.setItem(key, JSON.stringify(val));
  }
}

/**
 * @param {(string|!Array<string>)} items
 * @return {!Promise}
 */
export async function remove(items) {
  if (typeof items === 'string') {
    items = [items];
  }
  for (const key of items) {
    window.localStorage.removeItem(key);
  }
}

/**
 * @return {!Promise}
 */
export async function clear() {
  window.localStorage.clear();
}
