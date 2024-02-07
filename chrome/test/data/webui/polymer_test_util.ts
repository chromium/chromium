// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {afterNextRender, beforeNextRender, flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Converts beforeNextRender() API to promise-based.
 */
export function waitBeforeNextRender(element: HTMLElement): Promise<void> {
  return new Promise(resolve => {
    beforeNextRender(element, resolve);
  });
}

/**
 * Converts afterNextRender() API to promise-based.
 */
export function waitAfterNextRender(element: HTMLElement): Promise<void> {
  return new Promise(resolve => {
    afterNextRender(element, resolve);
  });
}

/*
 * Waits for queued up tasks to finish before proceeding. Inspired by:
 * https://github.com/Polymer/web-component-tester/blob/master/browser/environment/helpers.js#L97
 */
export function flushTasks(): Promise<void> {
  flush();
  // Promises have microtask timing, so we use setTimeout to explicitly force
  // a new task.
  return new Promise(function(resolve) {
    window.setTimeout(resolve, 1);
  });
}

/**
 * Data-binds two Polymer properties using the property-changed events and
 * set/notifyPath API. Useful for testing components which would normally be
 * used together.
 */
export function fakeDataBind(
    el1: PolymerElement, el2: PolymerElement, property: string) {
  type PropertyChangedEvent = CustomEvent<{path: string, value: any}>;

  const forwardChange = function(
      el: PolymerElement, event: PropertyChangedEvent) {
    if (event.detail.hasOwnProperty('path')) {
      el.notifyPath(event.detail.path, event.detail.value);
    } else {
      el.set(property, event.detail.value);
    }
  };
  // Add the listeners symmetrically. Polymer will prevent recursion.
  el1.addEventListener(
      property + '-changed',
      e => forwardChange(el2, e as PropertyChangedEvent));
  el2.addEventListener(
      property + '-changed',
      e => forwardChange(el1, e as PropertyChangedEvent));
}
