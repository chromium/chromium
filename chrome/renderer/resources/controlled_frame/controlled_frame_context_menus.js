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
const WebUrlPatternNatives = requireNative('WebUrlPatternNatives');
const convertURLPatternsToMatchPatterns =
    require('controlledFrameURLPatternsHelper')
        .convertURLPatternsToMatchPatterns;

function identity(value) {
  return value;
}

function ensureString(value) {
  return String(value);
}

function extractAndMapValues(obj, mapping) {
  const mapped = {__proto__: null};
  for (const [key, value] of $Object.entries(obj)) {
    if (key in mapping) {
      $Object.defineProperty(mapped, key, {
        __proto__: null,
        value: mapping[key](value),
        enumerable: true,
        configurable: true,
      });
    }
  }
  return mapped;
}

function renameObjectKeys(obj, mapping) {
  for (const [oldKey, newKey] of $Object.entries(mapping)) {
    if (oldKey in obj) {
      $Object.defineProperty(obj, newKey, {
        __proto__: null,
        value: obj[oldKey],
        enumerable: true,
        configurable: true,
      });
      delete obj[oldKey];
    }
  }
}

function ControlledFrameContextMenusImpl(webView, viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
  $Function.apply(WebViewContextMenusImpl, this, [webView, viewInstanceId]);
}
$Object.setPrototypeOf(
    ControlledFrameContextMenusImpl.prototype,
    WebViewContextMenusImpl.prototype);

function getCallbackIndex(name) {
  let foundMethodDetails = undefined;

  foundMethodDetails = $Array.find(
      CHROME_WEB_VIEW_CONTEXT_MENUS_PROMISE_API_METHODS,
      el => el.name === name);

  logging.CHECK(
      foundMethodDetails !== undefined,
      'could not find context menus method details');
  return foundMethodDetails.callbackIndex;
}

ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased =
    function(handler, name) {
  const callbackIndex = getCallbackIndex(name);
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
};

// Controlled Frame has its own internal definition of Context Menus
// create().
ControlledFrameContextMenusImpl.prototype.createImpl = function() {
  const args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(
      ControlledFrameInternal.contextMenusCreate, null, args);
};

// Controlled Frame has its own internal definition of Context Menus
// update().
ControlledFrameContextMenusImpl.prototype.updateImpl = function() {
  const args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(
      ControlledFrameInternal.contextMenusUpdate, null, args);
};

ControlledFrameContextMenusImpl.prototype.create =
    ControlledFrameContextMenusImpl.prototype.convertMethodToPromiseBased(
        ControlledFrameContextMenusImpl.prototype.createImpl, 'create');

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

function unwebifyContextMenusProperties(properties) {
  const unwebifiedProperties = extractAndMapValues(properties, {
    checked: identity,
    contexts: identity,
    documentURLPatterns: $Function.bind(
      convertURLPatternsToMatchPatterns, null),
    enabled: identity,
    parentId: identity,
    targetURLPatterns: $Function.bind(
      convertURLPatternsToMatchPatterns, null),
    title: identity,
    type: identity,
  });

  renameObjectKeys(unwebifiedProperties, {
    __proto__: null,
    documentURLPatterns: 'documentUrlPatterns',
    targetURLPatterns: 'targetUrlPatterns',
  });
  return unwebifiedProperties;
}

function unwebifyContextMenusCreateProperties(properties) {
  const unwebifiedProperties = extractAndMapValues(properties, {
    id: identity,
    checked: identity,
    contexts: identity,
    documentURLPatterns: $Function.bind(
      convertURLPatternsToMatchPatterns, null),
    enabled: identity,
    parentId: identity,
    targetURLPatterns: $Function.bind(
      convertURLPatternsToMatchPatterns, null),
    title: identity,
    type: identity,
  });

  renameObjectKeys(unwebifiedProperties, {
    __proto__: null,
    documentURLPatterns: 'documentUrlPatterns',
    targetURLPatterns: 'targetUrlPatterns',
  });
  return unwebifiedProperties;
}

function webifyClickEventDetails(details) {
  const webDetails = extractAndMapValues(details, {
    frameId: identity,
    frameUrl: identity,
    pageUrl: identity,
    editable: identity,
    linkUrl: identity,
    mediaType: identity,
    selectionText: identity,
    srcUrl: identity,
  });

  const webMenuItem = extractAndMapValues(details, {
    menuItemId: ensureString,
    parentMenuId: ensureString,
    checked: identity,
    wasChecked: identity,
  });

  renameObjectKeys(webMenuItem, {
    __proto__: null,
    menuItemId: 'id',
  });

  $Object.defineProperty(webDetails, 'menuItem', {
    __proto__: null,
    value: new MenuItemDetails(webMenuItem),
    enumerable: true,
    configurable: true,
  });

  renameObjectKeys(webDetails, {
    __proto__: null,
    frameUrl: 'frameURL',
    pageUrl: 'pageURL',
    linkUrl: 'linkURL',
    srcUrl: 'srcURL',
  });
  return webDetails;
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

  create(...args) {
    const [properties, ...remainingArgs] = args;
    if (properties === undefined) {
      return Promise.reject(
          new Error('Cannot create context menu without properties.'));
    }

    return this.#contextMenusImpl.create(
        unwebifyContextMenusCreateProperties(properties), ...remainingArgs);
  }
  update(...args) {
    const [id, properties, ...remainingArgs] = args;
    if (id === undefined || properties === undefined) {
      return Promise.reject(
          new Error('Cannot update context menu without id and properties.'));
    }
    return this.#contextMenusImpl.update(
        id, unwebifyContextMenusProperties(properties), ...remainingArgs);
  }
  remove(...args) {
    const [id, ...remainingArgs] = args;
    if (id === undefined) {
      return Promise.reject(
          new Error('Cannot remove entry context menu without id.'));
    }
    return this.#contextMenusImpl.remove(id, ...remainingArgs);
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
    switch (type) {
      case 'show':
        // No mapping needed for the show event as it is speced as a plain
        // event.
        menuEvent = new ContextMenusShowEvent(details);
        break;
      case 'click':
        menuEvent =
            new ContextMenusClickEvent(webifyClickEventDetails(details));
        break;
    }

    const listenerReturnValue = listener(menuEvent);
    if (listenerReturnValue instanceof Promise) {
      console.error(`ContextMenus ${type} handlers must be synchronous`);
    }
    return {__proto__: null};
  }
}

class MenuItemDetails {
  constructor(details) {
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

class ContextMenusShowEvent extends Event {
  constructor(details) {
    super('show');
    this['preventDefault'] = $Function.bind(details.preventDefault, this);
    $Object.freeze(this);
  }
}

class ContextMenusClickEvent extends Event {
  constructor(details) {
    super('click');
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

// Exports.
exports.$set('ControlledFrameContextMenus', ControlledFrameContextMenus);
