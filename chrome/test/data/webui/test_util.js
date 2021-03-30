// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {afterNextRender, beforeNextRender, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
// clang-format on

// Do not depend on the Chai Assertion Library in this file. Some consumers of
// the following test utils are not configured to use Chai.

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
      return target.getAttribute(attributeName) === attributeValue;
    }

    return isDone() ? Promise.resolve() : new Promise(function(resolve) {
      new MutationObserver(function(mutations, observer) {
        for (const mutation of mutations) {
          if (mutation.type === 'attributes' &&
              mutation.attributeName === attributeName && isDone()) {
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
   * Observes an HTML element and fires a promise when the check function is
   * satisfied.
   * @param {!HTMLElement} target
   * @param {Function} check
   * @return {!Promise}
   */
  /* #export */ function whenCheck(target, check) {
    return check() ?
        Promise.resolve() :
        new Promise(resolve => new MutationObserver((list, observer) => {
                                 if (check()) {
                                   observer.disconnect();
                                   resolve();
                                 }
                               }).observe(target, {
          attributes: true,
          childList: true,
          subtree: true
        }));
  }

  /**
   * Converts an event occurrence to a promise.
   * @param {string} eventType
   * @param {!Element|!EventTarget|!Window} target
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
   * @param {!Element} el1
   * @param {!Element} el2
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
   * Returns whether or not the element specified is visible.
   * @param {!HTMLElement} element
   * @return {boolean}
   */
  /* #export */ function isVisible(element) {
    const rect = element ? element.getBoundingClientRect() : null;
    return (!!rect && rect.width * rect.height > 0);
  }

  /**
   * Searches the DOM of the parentEl element for a child matching the provided
   * selector then checks the visibility of the child.
   * @param {!HTMLElement} parentEl
   * @param {string} selector
   * @param {boolean=} checkLightDom
   * @return {boolean}
   */
  /* #export */ function isChildVisible(parentEl, selector, checkLightDom) {
    const element = (checkLightDom ? parentEl.querySelector : parentEl.$$)
                        .call(parentEl, selector);
    return isVisible(element);
  }

  // #cr_define_end
  return {
    eventToPromise: eventToPromise,
    fakeDataBind: fakeDataBind,
    flushTasks: flushTasks,
    isVisible: isVisible,
    isChildVisible: isChildVisible,
    waitAfterNextRender: waitAfterNextRender,
    waitBeforeNextRender: waitBeforeNextRender,
    whenAttributeIs: whenAttributeIs,
  };
});
