// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a ControlledFrameWebRequest class that wraps WebView's
// WebRequest implementation and provides a more web-friendly API that uses
// EventTarget and Web naming conventions for enums. ControlledFrameWebRequest
// doesn't provide any new functionality; it translates its API to the
// WebView API.

const $Headers = require('safeMethods').SafeMethods.$Headers;

const convertURLPatternsToMatchPatterns =
    require('controlledFrameURLPatternsHelper')
        .convertURLPatternsToMatchPatterns;

function convertExtensionHeadersToWeb(httpHeaders) {
  const headers = new $Headers.self();
  for (const header of httpHeaders) {
    const value = (header.value !== undefined) ?
        header.value :
        $String.fromCharCode(...header.binaryValue);
    $Headers.append(headers, header.name, value);
  }
  return headers;
}

function convertWebHeadersToExtension(headersInit) {
  const headers = new $Headers.self(headersInit);
  const httpHeaders = $Array.self();
  $Headers.forEach(headers, (value, key) => {
    $Array.push(httpHeaders, {
      __proto__: null,
      name: key,
      value,
    });
  });
  return httpHeaders;
}

function identity(value) {
  return value;
}

function mapString(mapping, value) {
  if (value in mapping) {
    return mapping[value];
  }
  return value;
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

function emptyObjectToUndefined(obj) {
  if ($Object.keys(obj).length === 0) {
    return undefined;
  }
  return obj;
}

// Converts the extensions WebRequest details object to the format used by
// Controlled Frame, which is more grouped and follows web naming conventions.
function webifyRequestDetails(details) {
  const webDetails = extractAndMapValues(details, {
    documentId: identity,
    documentLifecycle: $Function.bind(
        mapString, null,
        {__proto__: null, pending_deletion: 'pending-deletion'}),
    error: identity,
    frameId: identity,
    frameType: $Function.bind(mapString, null, {
      __proto__: null,
      outermost_frame: 'outermost-frame',
      fenced_frame: 'fenced-frame',
      sub_frame: 'sub-frame',
    }),
    parentDocumentId: identity,
    parentFrameId: identity,
  });

  const request = extractAndMapValues(details, {
    initiator: identity,
    method: identity,
    requestBody: identity,
    requestHeaders: convertExtensionHeadersToWeb,
    requestId: identity,
    type: $Function.bind(mapString, null, {
      __proto__: null,
      main_frame: 'main-frame',
      sub_frame: 'sub-frame',
      csp_report: 'csp-report',
    }),
    url: identity,
  });
  renameObjectKeys(request, {
    __proto__: null,
    requestBody: 'body',
    requestHeaders: 'headers',
    requestId: 'id',
  });
  $Object.freeze(request);

  const response = emptyObjectToUndefined(extractAndMapValues(details, {
    __proto__: null,
    fromCache: identity,
    ip: identity,
    redirectUrl: identity,
    responseHeaders: convertExtensionHeadersToWeb,
    statusCode: identity,
    statusLine: identity,
  }));
  if (response) {
    $Object.defineProperty(response, 'auth', {
      __proto__: null,
      value: emptyObjectToUndefined(extractAndMapValues(details, {
        __proto__: null,
        challenger: identity,
        isProxy: identity,
        realm: identity,
        scheme: identity,
      })),
      enumerable: true,
    });
    renameObjectKeys(response, {
      __proto__: null,
      redirectUrl: 'redirectURL',
      responseHeaders: 'headers',
    });
  }
  $Object.freeze(response);

  return $Object.assign(webDetails, {__proto__: null, request, response});
}

class ControlledFrameWebRequest {
  #webRequest;

  constructor(webRequest) {
    this.#webRequest = webRequest;
  }

  createWebRequestInterceptor(options) {
    if (!options.urlPatterns || options.urlPatterns.length === 0) {
      throw new TypeError('"urlPatterns" must contain at least one value');
    }
    if (options.includeHeaders !== undefined &&
        !$Array.includes(
            $Array.self('none', 'cors', 'all'), options.includeHeaders)) {
      throw new TypeError(
          'If defined, "includeHeaders" must equal the string ' +
          '"none", "cors", or "all".');
    }
    return new WebRequestInterceptor(this.#webRequest, options);
  }
}

function createEventInfo(webRequestEventName) {
  return {
    webRequestEventName,
    registeredListeners: $Object.create(null),
  };
}

class WebRequestInterceptor extends EventTarget {
  #webRequest;
  #filter;
  #extraInfoSpec;

  #events = {
    authrequired: createEventInfo('onAuthRequired'),
    beforeredirect: createEventInfo('onBeforeRedirect'),
    beforerequest: createEventInfo('onBeforeRequest'),
    beforesendheaders: createEventInfo('onBeforeSendHeaders'),
    completed: createEventInfo('onCompleted'),
    erroroccurred: createEventInfo('onErrorOccurred'),
    headersreceived: createEventInfo('onHeadersReceived'),
    responsestarted: createEventInfo('onResponseStarted'),
    sendheaders: createEventInfo('onSendHeaders'),
  };

  constructor(webRequest, options) {
    super();
    this.#webRequest = webRequest;

    this.#filter = {
      __proto__: null,
      urls: convertURLPatternsToMatchPatterns(options.urlPatterns),
    };
    if (options.resourceTypes !== undefined) {
      this.#filter.types =
          $Array.map(options.resourceTypes, $Function.bind(mapString, null, {
            __proto__: null,
            'main-frame': 'main_frame',
            'sub-frame': 'sub_frame',
            'csp-report': 'csp_report',
          }));
    }

    // All possibly valid extraInfoSpec values are added here. When registering
    // handlers, this list will be filtered to those supported by the specific
    // event type.
    this.#extraInfoSpec = $Array.self();
    if (options.blocking === true) {
      $Array.push(this.#extraInfoSpec, 'blocking');
      $Array.push(this.#extraInfoSpec, 'asyncBlocking');
    }
    if (options.includeRequestBody === true) {
      $Array.push(this.#extraInfoSpec, 'requestBody');
    }
    if (options.includeHeaders === 'cors') {
      $Array.push(this.#extraInfoSpec, 'requestHeaders');
      $Array.push(this.#extraInfoSpec, 'responseHeaders');
    } else if (options.includeHeaders === 'all') {
      $Array.push(this.#extraInfoSpec, 'requestHeaders');
      $Array.push(this.#extraInfoSpec, 'responseHeaders');
      $Array.push(this.#extraInfoSpec, 'extraHeaders');
    }
  }

  addEventListener(type, webListener, options) {
    const eventInfo = this.#events[type];
    if (eventInfo === undefined) {
      $Function.apply(super.addEventListener, this, arguments);
      return;
    }

    const supportedExtraInfoSpec = {
      __proto__: null,
      // authrequired always uses 'asyncBlocking' instead of 'blocking'.
      authrequired:
          $Array.self('asyncBlocking', 'responseHeaders', 'extraHeaders'),
      beforeredirect: $Array.self('responseHeaders', 'extraHeaders'),
      beforerequest: $Array.self('blocking', 'requestBody'),
      beforesendheaders:
          $Array.self('blocking', 'requestHeaders', 'extraHeaders'),
      completed: $Array.self('responseHeaders', 'extraHeaders'),
      erroroccurred: $Array.self(),
      headersreceived:
          $Array.self('blocking', 'responseHeaders', 'extraHeaders'),
      responsestarted: $Array.self('responseHeaders', 'extraHeaders'),
      sendheaders: $Array.self('requestHeaders', 'extraHeaders'),
    }[type];
    const filteredExtraInfoSpec = $Array.filter(
        this.#extraInfoSpec,
        (flag) => $Array.includes(supportedExtraInfoSpec, flag));

    const blocking = $Array.includes(filteredExtraInfoSpec, 'blocking') ||
        $Array.includes(filteredExtraInfoSpec, 'asyncBlocking');
    const webRequestListener =
        $Function.bind(this.#onEvent, this, type, webListener, blocking);
    eventInfo.registeredListeners[webListener] = webRequestListener;
    this.#webRequest[eventInfo.webRequestEventName].addListener(
        webRequestListener, this.#filter, filteredExtraInfoSpec);
  }

  removeEventListener(type, webListener, options) {
    const eventInfo = this.#events[type];
    if (eventInfo === undefined) {
      $Function.apply(super.removeEventListener, this, arguments);
      return;
    }

    if (webListener in eventInfo.registeredListeners) {
      this.#webRequest[eventInfo.webRequestEventName].removeListener(
          eventInfo.registeredListeners[webListener]);
      delete eventInfo.registeredListeners[webListener];
    }
  }

  #onEvent(type, webListener, blocking, details, asyncCallback) {
    let webEvent;
    const webDetails = webifyRequestDetails(details);
    const result = blocking ? {__proto__: null} : undefined;
    switch (type) {
      case 'authrequired':
        if (blocking && asyncCallback) {
          this.#handleAsyncAuthRequiredEvent(
              webListener, webDetails, asyncCallback);
          return;
        }
        webEvent = new AuthRequiredEvent(webDetails, result, {__proto__: null});
        break;
      case 'beforeredirect':
        webEvent = new BeforeRedirectEvent(webDetails);
        break;
      case 'beforerequest':
        webEvent = new BeforeRequestEvent(webDetails, result);
        break;
      case 'beforesendheaders':
        webEvent = new BeforeSendHeadersEvent(webDetails, result);
        break;
      case 'completed':
        webEvent = new CompletedEvent(webDetails);
        break;
      case 'erroroccurred':
        webEvent = new ErrorOccurredEvent(webDetails);
        break;
      case 'headersreceived':
        webEvent = new HeadersReceivedEvent(webDetails, result);
        break;
      case 'responsestarted':
        webEvent = new ResponseStartedEvent(webDetails);
        break;
      case 'sendheaders':
        webEvent = new SendHeadersEvent(webDetails);
        break;
    }
    const listenerReturnValue = webListener(webEvent);
    if (listenerReturnValue instanceof Promise) {
      console.error(`WebRequest ${type} handlers must be synchronous`);
    }
    return result;
  }

  #handleAsyncAuthRequiredEvent(webListener, webDetails, asyncCallback) {
    const result = {__proto__: null};
    const options = {__proto__: null};
    const webEvent = new AuthRequiredEvent(webDetails, result, options);
    const listenerReturnValue = webListener(webEvent);
    if (listenerReturnValue instanceof $Promise.self) {
      console.error(`authrequired handlers must be synchronous`);
    }

    if (result.cancel || (options.signal && options.signal.aborted)) {
      asyncCallback({__proto__: null, cancel: true});
      return;
    }

    if (!result.authCredentials) {
      asyncCallback();
      return;
    }

    const resultPromises =
        $Array.self($Promise.resolve(result.authCredentials));
    if (options.signal) {
      $Array.push(resultPromises, new $Promise.self((resolve) => {
        options.signal.addEventListener('abort', resolve);
      }));
    }

    const promise = $Promise.race(resultPromises);
    $Promise.then(promise, (authCredentials) => {
      const response = {__proto__: null};
      if (options.signal && options.signal.aborted) {
        response.cancel = true;
      } else {
        response.authCredentials = authCredentials;
      }
      asyncCallback(response);
    });
    $Promise.catch(promise, (e) => {
      console.error('authrequired Promise rejected:', e);
      asyncCallback();
    });
  }
}

class AuthRequiredEvent extends Event {
  #result;
  #options;

  constructor(details, result, options) {
    super('authrequired');
    $Object.assign(this, details);
    this.#result = result;
    this.#options = options;
    $Object.freeze(this);
  }

  preventDefault() {
    if (this.#result !== undefined) {
      this.#result.cancel = true;
    }
    super.preventDefault();
  }

  setCredentials(credentials, options) {
    if (this.#result === undefined) {
      console.error(
          'AuthRequiredEvent.setCredentials is only supported ' +
          'in blocking event handlers');
      return;
    }
    this.#result.authCredentials = credentials;
    if (options && options.signal) {
      if (options.signal instanceof AbortSignal) {
        this.#options.signal = options.signal;
      } else {
        console.error(
            'options.signal argument to setCredentials ' +
            'must be an AbortSignal');
      }
    }
  }
}

class BeforeRedirectEvent extends Event {
  constructor(details) {
    super('beforeredirect');
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

class BeforeRequestEvent extends Event {
  #result;

  constructor(details, result) {
    super('beforerequest');
    $Object.assign(this, details);
    this.#result = result;
    $Object.freeze(this);
  }

  preventDefault() {
    if (this.#result !== undefined) {
      this.#result.cancel = true;
    }
    super.preventDefault();
  }

  redirect(url) {
    if (this.#result === undefined) {
      console.error(
          'BeforeRequestEvent.redirect is only supported ' +
          'in blocking event handlers');
      return;
    }
    this.#result.redirectUrl = url;
  }
}

class BeforeSendHeadersEvent extends Event {
  #result;

  constructor(details, result) {
    super('authrequired');
    $Object.assign(this, details);
    this.#result = result;
    $Object.freeze(this);
  }

  preventDefault() {
    if (this.#result !== undefined) {
      this.#result.cancel = true;
    }
    super.preventDefault();
  }

  setRequestHeaders(headersInit) {
    if (this.#result === undefined) {
      console.error(
          'BeforeSendHeadersEvent.setRequestHeaders is only supported ' +
          'in blocking event handlers');
      return;
    }
    this.#result.requestHeaders = convertWebHeadersToExtension(headersInit);
  }
}

class CompletedEvent extends Event {
  constructor(details) {
    super('completed');
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

class ErrorOccurredEvent extends Event {
  constructor(details) {
    super('erroroccurred');
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

class HeadersReceivedEvent extends Event {
  #result;

  constructor(details, result) {
    super('headersreceived');
    $Object.assign(this, details);
    this.#result = result;
    $Object.freeze(this);
  }

  preventDefault() {
    if (this.#result !== undefined) {
      this.#result.cancel = true;
    }
    super.preventDefault();
  }

  redirect(url) {
    if (this.#result === undefined) {
      console.error(
          'HeadersReceivedEvent.redirect is only supported ' +
          'in blocking event handlers');
      return;
    }
    this.#result.redirectUrl = url;
  }

  setResponseHeaders(headersInit) {
    if (this.#result === undefined) {
      console.error(
          'HeadersReceivedEvent.setResponseHeaders is only supported ' +
          'in blocking event handlers');
      return;
    }
    this.#result.responseHeaders = convertWebHeadersToExtension(headersInit);
  }
}

class ResponseStartedEvent extends Event {
  constructor(details) {
    super('responsestarted');
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

class SendHeadersEvent extends Event {
  constructor(details) {
    super('sendheaders');
    $Object.assign(this, details);
    $Object.freeze(this);
  }
}

// Exports.
exports.$set('ControlledFrameWebRequest', ControlledFrameWebRequest);
