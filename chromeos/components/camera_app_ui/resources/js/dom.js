// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './chrome_util.js';

// Disables eslint check for closure compiler constructor type.
/* eslint-disable valid-jsdoc */

/**
 * Gets an element matching css selector under the target element and checks its
 * type.
 * @param {!Node} target
 * @param {string} selector
 * @param {function(new: T, ...)} type The expected element type.
 * @return {T}
 * @template T
 */
export function getFrom(target, selector, type) {
  return assertInstanceof(target.querySelector(selector), type);
}

/**
 * Gets all elements matching css selector under the target element and asserts
 * their type to be specific type.
 * @param {!Node} target
 * @param {string} selector
 * @param {function(new: T, ...)} type The expected element type.
 * @return {!NodeList<T>}
 * @template T
 */
export function getAllFrom(target, selector, type) {
  const elements = target.querySelectorAll(selector);
  for (const el of elements) {
    assertInstanceof(el, type);
  }
  return elements;
}

/**
 * Gets an element in document matching css selector and checks its type.
 * @param {string} selector
 * @param {function(new: T, ...)} type The expected element type.
 * @return {T}
 * @template T
 */
export function get(selector, type) {
  return getFrom(document, selector, type);
}

/**
 * Gets all elements in document matching css selector and asserts their type to
 * be specific type.
 * @param {string} selector
 * @param {function(new: T, ...)} type The expected element type.
 * @return {!NodeList<T>}
 * @template T
 */
export function getAll(selector, type) {
  return getAllFrom(document, selector, type);
}

/**
 * Creates a typed element.
 * @param {string} tag The HTML tag of the element to be created.
 * @param {function(new: T, ...)} type The expected element type.
 * @return {!T}
 * @template T
 */
export function create(tag, type) {
  const el = document.createElement(tag);
  return assertInstanceof(el, type);
}

/* eslint-enable valid-jsdoc */
