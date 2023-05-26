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

// The implementation of the cr module is located in
// //components/translate/core/browser/resources/translate.js
declare module cr {
  let googleTranslate: any;
}

let securityOrigin: string;

/**
 * Translate XMLHttpRequests still outstanding.
 */
const xhrs: TranslateHttpRequest[] = [];

class TranslateHttpRequest extends XMLHttpRequest {
  savedMethod = '';
  savedUrl = '';

  /**
   * Redefine XMLHttpRequest's open to capture request configurations.
   */
  override open(method: string, url: string|URL): void;
  override open(
      method: string, url: string|URL, isAsync: boolean,
      username?: string|null|undefined, password?: string|null|undefined): void;
  override open(
      method: string, url: string|URL, isAsync?: boolean,
      username?: string|null|undefined,
      password?: string|null|undefined): void {
    this.savedMethod = method;
    this.savedUrl = url as string;
    const realAsync = isAsync ? isAsync : true;
    super.open(this.savedMethod, this.savedUrl, realAsync, username, password);
  }

  /**
   * Redefine XMLHttpRequest's send to call into the browser if it matches the
   * predefined translate security origin.
   */
  override send(body?: Document|XMLHttpRequestBodyInit|null|undefined): void {
    if (this.savedUrl.startsWith(securityOrigin)) {
      const length = xhrs.push(this);
      sendWebKitMessage('TranslateMessage', {
        'command': 'sendrequest',
        'method': this.savedMethod,
        'url': this.savedUrl,
        'body': body,
        'requestID': length - 1,
      });
    } else {
      super.send(body);
    }
  }
}

XMLHttpRequest = TranslateHttpRequest;

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
  cr.googleTranslate.loadJavascriptCallback = function(url: string) {
    sendWebKitMessage(
        'TranslateMessage', {'command': 'loadjavascript', 'url': url});
  };
}

/**
 * Receives the response the browser got for the proxied xhr request and
 * configures the xhr object as it would have been had it been sent normally.
 * @param url The original url which initiated the request.
 * @param requestID The index of the xhr request in |xhrs|.
 * @param status HTTP response code.
 * @param statusText HTTP response status text.
 * @param responseURL The url which the response was returned from.
 * @param responseText The text received from the server.
 */
function handleResponse(
    requestID: number, status: number, statusText: string, responseURL: string,
    responseText: string) {
  // Retrieve xhr object that's waiting for the response.
  const xhr = xhrs[requestID];

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

  xhr!.onreadystatechange?.(new Event(''));

  // Clean it up
  delete xhrs[requestID];
}

// Mark: Public API
gCrWeb.translate = {
  installCallbacks,
  handleResponse,
};
