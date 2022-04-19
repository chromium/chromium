// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Translate script for iOS that is needed in addition to the
 * cross platform script translate.js.
 *
 * TODO(crbug.com/659442): Enable checkTypes, checkVars errors for this file.
 * @suppress {checkTypes, checkVars}
 */

// Requires functions from base.js

/**
 * Namespace for this module.
 */
__gCrWeb.translate = {};

// Store message namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['translate'] = __gCrWeb.translate;

/**
 * Defines function to install callbacks on cr.googleTranslate.
 * See translate_script.cc for usage.
 */
__gCrWeb.translate['installCallbacks'] = function() {
  /**
   * Sets a callback to inform host of the ready state of the translate element.
   */
  cr.googleTranslate.readyCallback = function() {
    __gCrWeb.message.invokeOnHost({
        'command': 'translate.ready',
        'errorCode': cr.googleTranslate.errorCode,
        'loadTime': cr.googleTranslate.loadTime,
        'readyTime': cr.googleTranslate.readyTime});
  };

  /**
   * Sets a callback to inform host of the result of translation.
   */
  cr.googleTranslate.resultCallback = function() {
    __gCrWeb.message.invokeOnHost({
      'command': 'translate.status',
      'errorCode': cr.googleTranslate.errorCode,
      'pageSourceLanguage': cr.googleTranslate.sourceLang,
      'translationTime': cr.googleTranslate.translationTime
    });
  };

  /**
   * Sets a callback to inform host to download javascript.
   */
  cr.googleTranslate.loadJavascriptCallback = function(url) {
    __gCrWeb.message.invokeOnHost({
        'command': 'translate.loadjavascript',
        'url': url});
  };
};

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
    this.realOpen(method, url, async, user, password);
  };
}

/**
 * Translate XMLHttpRequests still outstanding.
 * @type {Array<XMLHttpRequest>}
 */
__gCrWeb.translate['xhrs'] = [];

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
      const length = __gCrWeb.translate['xhrs'].push(this);
      __gCrWeb.message.invokeOnHost({
          'command': 'translate.sendrequest',
          'method': this.savedMethod,
          'url': this.savedUrl,
          'body': body,
          'requestID': length - 1});
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
__gCrWeb.translate['handleResponse'] = function(
    url, requestID, status, statusText, responseURL, responseText) {
  // Retrive xhr object that's waiting for the response.
  xhr = __gCrWeb.translate['xhrs'][requestID];

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
  delete __gCrWeb.translate['xhrs'][requestID];
};
