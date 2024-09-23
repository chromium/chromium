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
}

function startTranslation(sourceLanguage: string, targetLanguage: string) {
  cr.googleTranslate.translate(sourceLanguage, targetLanguage);
}

function revertTranslation() {
  try {
    cr.googleTranslate.revert();
  } catch {
    // No op.
  }
}

// Mark: Public API
gCrWeb.translate = {
  installCallbacks,
  startTranslation,
  revertTranslation,
};
