// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on
// //extensions/test/data/web_view/no_internal_calls_to_user_code/main.js.

// These are needed for the test itself, so keep a reference to the real method.
EventTarget.prototype.savedAddEventListener =
    EventTarget.prototype.addEventListener;
document.body.savedAppendChild = document.body.appendChild;
Document.prototype.savedCreateElement = Document.prototype.createElement;

function makeUnreached() {
  return function unreachableFunction() {
    throw new Error("Reached unreachable code");
  };
}

// Step 1: Overwrite prototype method setters/getters on properties.
(function taintProperties() {
  var properties = [
    'AppView',
    'ControlledFrame',
    'WebView',
    '__proto__',
    'actionQueue',
    'allowscaling',
    'allowtransparency',
    'app',
    'appview',
    'attributes',
    'autosize',
    'border',
    'cancelable',
    'constructor',
    'contentWindow',
    'controlledframe',
    'data',
    'defaultView',
    'dirty',
    'element',
    'elementHeight',
    'errorNode',
    'events',
    'guest',
    'guestView',
    'height',
    'initialZoomFactor',
    'innerText',
    'instanceId',
    'internal',
    'internalInstanceId',
    'left',
    'listener',
    'loadstop',
    'maxheight',
    'newHeight',
    'on',
    'onloadstop',
    'onresize',
    'ownerDocument',
    'parentNode',
    'partition',
    'pendingAction',
    'position',
    'processId',
    'prototype',
    'shadowRoot',
    'src',
    'state',
    'style',
    'top',
    'userAgentOverride',
    'validPartitionId',
    'view',
    'viewInstanceId',
    'viewType',
    'webview',
  ];

  // For objects that don't inherit directly from Object, we'll need to taint
  // existing properties on prototypes earlier in the prototype chain.
  var otherConstructors = [
    Document,
    Element,
    HTMLElement,
    HTMLIFrameElement,
    Node,
  ];

  for (var property of properties) {
    Object.defineProperty(Object.prototype, property, {
      get: makeUnreached(),
      set: makeUnreached(),
    });
    for (var constructor of otherConstructors) {
      if (constructor.prototype.hasOwnProperty(property)) {
        Object.defineProperty(constructor.prototype, property, {
          get: makeUnreached(),
          set: makeUnreached(),
        });
      }
    }
  }
})();

// Step 2: Overwrite remainder of prototype methods.
Object.prototype.hasOwnProperty = makeUnreached();
Function.prototype.apply = makeUnreached();
Function.prototype.bind = makeUnreached();
Function.prototype.call = makeUnreached();
Array.prototype.concat = makeUnreached();
Array.prototype.filter = makeUnreached();
Array.prototype.forEach = makeUnreached();
Array.prototype.indexOf = makeUnreached();
Array.prototype.join = makeUnreached();
Array.prototype.map = makeUnreached();
Array.prototype.pop = makeUnreached();
Array.prototype.push = makeUnreached();
Array.prototype.reverse = makeUnreached();
Array.prototype.shift = makeUnreached();
Array.prototype.slice = makeUnreached();
Array.prototype.splice = makeUnreached();
Array.prototype.unshift = makeUnreached();
String.prototype.indexOf = makeUnreached();
String.prototype.replace = makeUnreached();
String.prototype.slice = makeUnreached();
String.prototype.split = makeUnreached();
String.prototype.substr = makeUnreached();
String.prototype.toLowerCase = makeUnreached();
String.prototype.toUpperCase = makeUnreached();
CustomElementRegistry.prototype.define = makeUnreached();
Document.prototype.createElement = makeUnreached();
Document.prototype.createEvent = makeUnreached();
Document.prototype.getElementsByTagName = makeUnreached();
Element.prototype.attachShadow = makeUnreached();
Element.prototype.getAttribute = makeUnreached();
Element.prototype.getBoundingClientRect = makeUnreached();
Element.prototype.hasAttribute = makeUnreached();
Element.prototype.removeAttribute = makeUnreached();
Element.prototype.setAttribute = makeUnreached();
EventTarget.prototype.addEventListener = makeUnreached();
EventTarget.prototype.dispatchEvent = makeUnreached();
EventTarget.prototype.removeEventListener = makeUnreached();
HTMLElement.prototype.focus = makeUnreached();
MutationObserver.prototype.observe = makeUnreached();
MutationObserver.prototype.takeRecords = makeUnreached();
Node.prototype.appendChild = makeUnreached();
Node.prototype.removeChild = makeUnreached();
Node.prototype.replaceChild = makeUnreached();

// Step 3: Overwrite constructors.
MutationObserver = makeUnreached();
Object = makeUnreached();
Function = makeUnreached();
Array = makeUnreached();
String = makeUnreached();

// Step 4: Overwrite static methods on constructors.
Object.assign = makeUnreached();
Object.create = makeUnreached();
Object.defineProperty = makeUnreached();
Object.freeze = makeUnreached();
Object.getOwnPropertyDescriptor = makeUnreached();
Object.getPrototypeOf = makeUnreached();
Object.keys = makeUnreached();
Object.setPrototypeOf = makeUnreached();
Array.from = makeUnreached();
Array.isArray = makeUnreached();

// Step 5: Overwrite global functions.
getComputedStyle = makeUnreached();
parseInt = makeUnreached();
parseFloat = makeUnreached();
