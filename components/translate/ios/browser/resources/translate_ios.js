// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Translate script for iOS that is needed in addition to the
 * cross platform script translate.js.
 *
 */

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Defines function to install callbacks on cr.googleTranslate.
 * See translate_script.cc for usage.
 */
function installCallbacks() {
  /**
   * Sets a callback to inform host of the ready state of the translate element.
   */
  cr.googleTranslate.readyCallback = function() {
    sendWebKitMessage('TranslateMessage', {
      'command': 'ready',
      'errorCode': cr.googleTranslate.errorCode,
      'loadTime': cr.googleTranslate.loadTime,
      'readyTime': cr.googleTranslate.readyTime,
    });
  };

  /**
   * Sets a callback to inform host of the result of translation.
   */
  cr.googleTranslate.resultCallback = function() {
    sendWebKitMessage('TranslateMessage', {
      'command': 'status',
      'errorCode': cr.googleTranslate.errorCode,
      'pageSourceLanguage': cr.googleTranslate.sourceLang,
      'translationTime': cr.googleTranslate.translationTime,
    });
  };

  /**
   * Sets a callback to inform host to download javascript.
   */
  cr.googleTranslate.loadJavascriptCallback = function(url) {
    sendWebKitMessage(
        'TranslateMessage', {'command': 'loadjavascript', 'url': url});
  };
}

/**
 * Redefine XMLHttpRequest's open to capture request configurations.
 * Only redefines once because this script may be injected multiple times.
 */
if (typeof XMLHttpRequest.prototype.realOpen === 'undefined') {
  XMLHttpRequest.prototype.realOpen = XMLHttpRequest.prototype.open;
  XMLHttpRequest.prototype.open = function(method, url, async, user, password) {
    this.savedMethod = method;
    this.savedUrl = url;
    this.savedAsync = async;
    this.savedUser = user;
    this.savedPassword = password;
    const realAsync = arguments.length > 2 ? async : true;
    this.realOpen(method, url, realAsync, user, password);
  };
}

/**
 * Redefine XMLHttpRequest's send to call into the browser if it matches the
 * predefined translate security origin.
 * Only redefines once because this script may be injected multiple times.
 */
if (typeof XMLHttpRequest.prototype.realSend === 'undefined') {
  XMLHttpRequest.prototype.realSend = XMLHttpRequest.prototype.send;
  XMLHttpRequest.prototype.send = function(body) {
    // If this is a translate request, save this xhr and proxy the request to
    // the browser. Else, pass it through to the original implementation.
    // |securityOrigin| is predefined by translate_script.cc.
    if (this.savedUrl.startsWith(securityOrigin)) {
      const length = gCrWeb.translate.xhrs.push(this);
      sendWebKitMessage('TranslateMessage', {
        'command': 'sendrequest',
        'method': this.savedMethod,
        'url': this.savedUrl,
        'body': body,
        'requestID': length - 1,
      });
    } else {
      this.realSend(body);
    }
  };
}

/**
 * Receives the response the browser got for the proxied xhr request and
 * configures the xhr object as it would have been had it been sent normally.
 * @param {string} url The original url which initiated the request.
 * @param {number} requestID The index of the xhr request in |xhrs|.
 * @param {number} status HTTP response code.
 * @param {string} statusText HTTP response status text.
 * @param {string} responseURL The url which the response was returned from.
 * @param {string} responseText The text received from the server.
 */
function handleResponse(
    url, requestID, status, statusText, responseURL, responseText) {
  // Retrieve xhr object that's waiting for the response.
  xhr = gCrWeb.translate.xhrs[requestID];

  // Configure xhr as it would have been if it was sent.
  Object.defineProperties(xhr, {
    responseText: {value: responseText},
    response: {value: responseText},
    readyState: {value: XMLHttpRequest.DONE},
    status: {value: status},
    statusText: {value: statusText},
    responseType: {value: 'text'},
    responseURL: {value: responseURL},
  });
  xhr.onreadystatechange();

  // Clean it up
  delete gCrWeb.translate.xhrs[requestID];
}

// Mark: Public API

/**
 * Translate XMLHttpRequests still outstanding.
 * @type {Array<XMLHttpRequest>}
 */
const xhrs = [];

gCrWeb.translate = {
  installCallbacks,
  handleResponse,
  xhrs,
};
