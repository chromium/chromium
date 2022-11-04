// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Installs Language Detection management functions on the
 * __gCrWeb object.
 */

__gCrWeb.languageDetection = {};

// Store languageDetection namespace object in a global __gCrWeb object
// referenced by a string, so it does not get renamed by closure compiler during
// the minification.
__gCrWeb['languageDetection'] = __gCrWeb.languageDetection;

/**
 * The cache of the text content that was extracted from the page
 */
__gCrWeb.languageDetection.bufferedTextContent = null;

/**
 * The number of active requests that have populated the cache. This is
 * incremented every time a call to |__gCrWeb.languageDetection.detectLanguage|
 * populates the buffer. This is decremented every time there is a call to
 * retrieve the buffer. The buffer is purged when this goes down to 0.
 */
__gCrWeb.languageDetection.activeRequests = 0;

/**
 * Searches page elements for "notranslate" meta tag.
 * @return {boolean} true if "notranslate" meta tag is defined.
 */
__gCrWeb.languageDetection['hasNoTranslate'] = function() {
  const metaTags = document.getElementsByTagName('meta');
  for (let i = 0; i < metaTags.length; ++i) {
    if (metaTags[i].name === 'google') {
      if (metaTags[i].content === 'notranslate' ||
          metaTags[i].getAttribute('value') === 'notranslate') {
        return true;
      }
    }
  }
  return false;
};

/**
 * Gets the content of a meta tag by httpEquiv.
 * The function is case insensitive.
 * @param {String} httpEquiv Value of the "httpEquiv" attribute, has to be
 *     lower case.
 * @return {string} Value of the "content" attribute of the meta tag.
 */
__gCrWeb.languageDetection['getMetaContentByHttpEquiv'] = function(httpEquiv) {
  const metaTags = document.getElementsByTagName('meta');
  for (let i = 0; i < metaTags.length; ++i) {
    if (metaTags[i].httpEquiv.toLowerCase() === httpEquiv) {
      return metaTags[i].content;
    }
  }
  return '';
};

// Used by the |getTextContent| function below.
__gCrWeb.languageDetection['nonTextNodeNames'] = {
  'SCRIPT': 1,
  'NOSCRIPT': 1,
  'STYLE': 1,
  'EMBED': 1,
  'OBJECT': 1,
};

/**
 * Walks a DOM tree to extract the text content.
 * Does not walk into a node when its name is in |nonTextNodeNames|.
 * @param {HTMLElement} node The DOM tree
 * @param {number} maxLen Output will be truncated to |maxLen|
 * @return {string} The text content
 */
__gCrWeb.languageDetection['getTextContent'] = function(node, maxLen) {
  if (!node || maxLen <= 0) {
    return '';
  }

  let txt = '';
  // Formatting and filtering.
  if (node.nodeType === Node.ELEMENT_NODE) {
    // Reject non-text nodes such as scripts.
    if (__gCrWeb.languageDetection.nonTextNodeNames[node.nodeName]) {
      return '';
    }
    if (node.nodeName === 'BR') {
      return '\n';
    }
    const style = window.getComputedStyle(node);
    // Only proceed if the element is visible.
    if (style.display === 'none' || style.visibility === 'hidden') {
      return '';
    }
    // No need to add a line break before |body| as it is the first element.
    if (node.nodeName.toUpperCase() !== 'BODY' && style.display !== 'inline') {
      txt = '\n';
    }
  }

  if (node.hasChildNodes()) {
    for (let childIdx = 0;
         childIdx < node.childNodes.length && txt.length < maxLen;
         childIdx++) {
      txt += __gCrWeb.languageDetection.getTextContent(
          node.childNodes[childIdx], maxLen - txt.length);
    }
  } else if (node.nodeType === Node.TEXT_NODE && node.textContent) {
    txt += node.textContent.substring(0, maxLen - txt.length);
  }

  return txt;
};

/**
 * Detects if a page has content that needs translation and informs the native
 * side. The text content of a page is cached in
 * |__gCrWeb.languageDetection.bufferedTextContent| and retrieved at a later
 * time directly from the Obj-C side. This is to avoid using |invokeOnHost|.
 */
__gCrWeb.languageDetection['detectLanguage'] = function() {
  // Constant for the maximum length of the extracted text returned by
  // |-detectLanguage| to the native side.
  // Matches desktop implementation.
  // Note: This should stay in sync with the constant in
  // ios_language_detection_tab_helper.mm .
  const kMaxIndexChars = 65535;
  const captureBeginTime = new Date();
  __gCrWeb.languageDetection.activeRequests += 1;
  __gCrWeb.languageDetection.bufferedTextContent =
      __gCrWeb.languageDetection.getTextContent(document.body, kMaxIndexChars);
  const captureTextTime =
      (new Date()).getMilliseconds() - captureBeginTime.getMilliseconds();
  const httpContentLanguage =
      __gCrWeb.languageDetection.getMetaContentByHttpEquiv('content-language');
  const textCapturedCommand = {
    'command': 'languageDetection.textCaptured',
    'hasNoTranslate': false,
    'captureTextTime': captureTextTime,
    'htmlLang': document.documentElement.lang,
    'httpContentLanguage': httpContentLanguage,
  };

  if (__gCrWeb.languageDetection.hasNoTranslate()) {
    textCapturedCommand['hasNoTranslate'] = true;
  }
  __gCrWeb.message.invokeOnHost(textCapturedCommand);
};

/**
 * Retrieves the cached text content of a page. Returns it and then purges the
 * cache.
 */
__gCrWeb.languageDetection['retrieveBufferedTextContent'] = function() {
  const textContent = __gCrWeb.languageDetection.bufferedTextContent;
  __gCrWeb.languageDetection.activeRequests -= 1;
  if (__gCrWeb.languageDetection.activeRequests === 0) {
    __gCrWeb.languageDetection.bufferedTextContent = null;
  }
  return textContent;
};
