// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a ControlledFrameContextMenus class that wraps WebView's
// ContextMenus implementation and provides a more web-friendly API that uses
// EventTarget and Web naming conventions for enums. ControlledFrameContextMenus
// doesn't provide any new functionality; it translates its API to the
// WebView API.

const CHROME_WEB_VIEW_CONTEXT_MENUS_PROMISE_API_METHODS =
    require('chromeWebViewContextMenusApiMethods').PROMISE_API_METHODS;
const logging = requireNative('logging');
const promiseWrap = require('guestViewContainerElement').promiseWrap;
const utils = require('utils');
const $Headers = require('safeMethods').SafeMethods.$Headers;
const WebViewContextMenusImpl =
    require('chromeWebView').WebViewContextMenusImpl;
const ControlledFrameInternal = getInternalApi('controlledFrameInternal');

function ControlledFrameContextMenusImpl(webView, viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
  $Function.apply(WebViewContextMenusImpl, this, [webView, viewInstanceId]);
}
$Object.setPrototypeOf(
    ControlledFrameContextMenusImpl.prototype,
    WebViewContextMenusImpl.prototype);

function getCallbackIndex(name) {
  let foundMethodDetails = undefined;
  for (const methodDetails of
           CHROME_WEB_VIEW_CONTEXT_MENUS_PROMISE_API_METHODS) {
    if (methodDetails.name === name) {
      foundMethodDetails = methodDetails;
      break;
    }
  }
  logging.CHECK(
      foundMethodDetails !== undefined,
      'could not find context menus method details');
  return foundMethodDetails.callbackIndex;
}

ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased =
    function(handler, name) {
  let callbackIndex = getCallbackIndex(name);
  // TODO(crbug.com/378956568): Verify these methods don't require an instance
  // ID check.
  function verifyEnvironment(reject) {
    return true;
  }
  return function(var_args) {
    return promiseWrap(
        handler.bind(this), arguments, callbackIndex, verifyEnvironment,
        /*callbackAllowed=*/ true);
  };
}

    // Controlled Frame has its own internal definition of Context Menus
    // create().
    ControlledFrameContextMenusImpl.prototype.createImpl =
        function() {
  const args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(
      ControlledFrameInternal.contextMenusCreate, null, args);
}

        // Controlled Frame has its own internal definition of Context Menus
        // update().
        ControlledFrameContextMenusImpl.prototype.updateImpl =
            function() {
  let args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(
      ControlledFrameInternal.contextMenusUpdate, null, args);
}

            ControlledFrameContextMenusImpl.prototype.create =
                ControlledFrameContextMenusImpl.prototype
                    .convertMethodToPromiseBased(
                        ControlledFrameContextMenusImpl.prototype.createImpl,
                        'create');

ControlledFrameContextMenusImpl.prototype.remove =
    ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased(
        WebViewContextMenusImpl.prototype.remove, 'remove');

ControlledFrameContextMenusImpl.prototype.removeAll =
    ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased(
        WebViewContextMenusImpl.prototype.removeAll, 'removeAll');

ControlledFrameContextMenusImpl.prototype.update =
    ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased(
        ControlledFrameContextMenusImpl.prototype.updateImpl, 'update');

function createEventInfo(contextMenusEventName) {
  return {
    contextMenusEventName,
    registeredListeners: $Object.create(null),
  };
}

function webifyEventDetails(event) {
  // TODO(crbug.com/429109629): Clarify the changes needed for the event and
  // implement them.
  return event;
}

class ControlledFrameContextMenus extends EventTarget {
  #contextMenusImpl;

  #events = {
    show: createEventInfo('onShow'),
    click: createEventInfo('onClicked'),
  };

  constructor(webView, viewInstanceId) {
    super();
    this.#contextMenusImpl =
        new ControlledFrameContextMenusImpl(webView, viewInstanceId);
  }

  // TODO(crbug.com/429109311): Implement enum mappings.
  create(...args) {
    return this.#contextMenusImpl.create(...args);
  }
  update(...args) {
    return this.#contextMenusImpl.update(...args);
  }
  remove(...args) {
    return this.#contextMenusImpl.remove(...args);
  }
  removeAll(...args) {
    return this.#contextMenusImpl.removeAll(...args);
  }

  addEventListener(type, listener, options) {
    const eventInfo = this.#events[type];
    if (eventInfo === undefined) {
      $Function.apply(super.addEventListener, this, arguments);
      return;
    }

    const contextMenusListener =
        $Function.bind(this.#onEvent, this, type, listener);
    eventInfo.registeredListeners[listener] = contextMenusListener;
    this.#contextMenusImpl[eventInfo.contextMenusEventName].addListener(
        contextMenusListener);
  }

  removeEventListener(type, listener, options) {
    const eventInfo = this.#events[type];
    if (eventInfo === undefined) {
      $Function.apply(super.removeEventListener, this, arguments);
      return;
    }

    if (listener in eventInfo.registeredListeners) {
      this.#contextMenusImpl[eventInfo.contextMenusEventName].removeListener(
          eventInfo.registeredListeners[listener]);
      delete eventInfo.registeredListeners[listener];
    }
  }

  #onEvent(type, listener, details) {
    let menuEvent;
    const webDetails = webifyEventDetails(details);
    switch (type) {
      case 'show':
        menuEvent = new ShowEvent(webDetails);
        break;
      case 'click':
        menuEvent = new ClickEvent(webDetails);
        break;
    }

    const listenerReturnValue = listener(menuEvent);
    if (listenerReturnValue instanceof Promise) {
      console.error(`ContextMenus ${type} handlers must be synchronous`);
    }
    return {__proto__: null};
  }
}

class ShowEvent extends Event {
  constructor(details) {
    super('show');
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

class ClickEvent extends Event {
  constructor(details) {
    super('click');
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

// Exports.
exports.$set('ControlledFrameContextMenus', ControlledFrameContextMenus);
