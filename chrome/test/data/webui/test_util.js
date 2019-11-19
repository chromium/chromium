// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {afterNextRender, beforeNextRender, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

cr.define('test_util', function() {
  /**
   * Observes an HTML attribute and fires a promise when it matches a given
   * value.
   * @param {!HTMLElement} target
   * @param {string} attributeName
   * @param {*} attributeValue
   * @return {!Promise}
   */
  /* #export */ function whenAttributeIs(
      target, attributeName, attributeValue) {
    function isDone() {
      return target.getAttribute(attributeName) == attributeValue;
    }

    return isDone() ? Promise.resolve() : new Promise(function(resolve) {
      new MutationObserver(function(mutations, observer) {
        for (const mutation of mutations) {
          assertEquals('attributes', mutation.type);
          if (mutation.attributeName == attributeName && isDone()) {
            observer.disconnect();
            resolve();
            return;
          }
        }
      }).observe(target, {
        attributes: true,
        childList: false,
        characterData: false
      });
    });
  }

  /**
   * Converts an event occurrence to a promise.
   * @param {string} eventType
   * @param {!HTMLElement} target
   * @return {!Promise} A promise firing once the event occurs.
   */
  /* #export */ function eventToPromise(eventType, target) {
    return new Promise(function(resolve, reject) {
      target.addEventListener(eventType, function f(e) {
        target.removeEventListener(eventType, f);
        resolve(e);
      });
    });
  }

  /**
   * Data-binds two Polymer properties using the property-changed events and
   * set/notifyPath API. Useful for testing components which would normally be
   * used together.
   * @param {!HTMLElement} el1
   * @param {!HTMLElement} el2
   * @param {string} property
   */
  /* #export */ function fakeDataBind(el1, el2, property) {
    const forwardChange = function(el, event) {
      if (event.detail.hasOwnProperty('path')) {
        el.notifyPath(event.detail.path, event.detail.value);
      } else {
        el.set(property, event.detail.value);
      }
    };
    // Add the listeners symmetrically. Polymer will prevent recursion.
    el1.addEventListener(property + '-changed', forwardChange.bind(null, el2));
    el2.addEventListener(property + '-changed', forwardChange.bind(null, el1));
  }

  /**
   * Converts beforeNextRender() API to promise-based.
   * @param {!Element} element
   * @return {!Promise}
   */
  /* #export */ function waitBeforeNextRender(element) {
    return new Promise(resolve => {
      Polymer.RenderStatus.beforeNextRender(element, resolve);
    });
  }

  /**
   * @param {!HTMLElement} element
   * @return {!Promise} Promise that resolves when an afterNextRender()
   *     callback on |element| is run.
   */
  /* #export */ function waitAfterNextRender(element) {
    return new Promise(resolve => {
      Polymer.RenderStatus.afterNextRender(element, resolve);
    });
  }

  /*
   * Waits for queued up tasks to finish before proceeding. Inspired by:
   * https://github.com/Polymer/web-component-tester/blob/master/browser/environment/helpers.js#L97
   */
  /* #export */ function flushTasks() {
    Polymer.dom.flush();
    // Promises have microtask timing, so we use setTimeout to explicitly force
    // a new task.
    return new Promise(function(resolve, reject) {
      window.setTimeout(resolve, 0);
    });
  }

  /**
   * Returns whether or not the element specified is visible. This is different
   * from isElementVisible in that this function attempts to search for the
   * element within a parent element, which means you can use it to check if
   * the element exists at all.
   * @param {!HTMLElement} parentEl
   * @param {string} selector
   * @param {boolean=} checkLightDom
   * @return {boolean}
   */
  /* #export */ function isVisible(parentEl, selector, checkLightDom) {
    const element = (checkLightDom ? parentEl.querySelector : parentEl.$$)
                        .call(parentEl, selector);
    const rect = element ? element.getBoundingClientRect() : null;
    return !!rect && rect.width * rect.height > 0;
  }

  // #cr_define_end
  return {
    eventToPromise: eventToPromise,
    fakeDataBind: fakeDataBind,
    flushTasks: flushTasks,
    isVisible: isVisible,
    waitAfterNextRender: waitAfterNextRender,
    waitBeforeNextRender: waitBeforeNextRender,
    whenAttributeIs: whenAttributeIs,
  };
});
