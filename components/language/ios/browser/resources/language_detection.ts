// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

// Mark: Private properties

/**
 * The cache of the text content that was extracted from the page
 */
let bufferedTextContent: string|null;

/**
 * The number of active requests that have populated the cache. This is
 * incremented every time a call to `__gCrWeb.languageDetection.detectLanguage`
 * populates the buffer. This is decremented every time there is a call to
 * retrieve the buffer. The buffer is purged when this goes down to 0.
 */
let activeRequests = 0;

/**
 * Searches page elements for "notranslate" meta tag.
 * @return  true if "notranslate" meta tag is defined.
 */
function hasNoTranslate(): boolean {
  for (const metaTag of document.getElementsByTagName('meta')) {
    if (metaTag.name === 'google') {
      if (metaTag.content === 'notranslate' ||
          metaTag.getAttribute('value') === 'notranslate') {
        return true;
      }
    }
  }
  return false;
}

/**
 * Gets the content of a meta tag by httpEquiv.
 * The function is case insensitive.
 * @param httpEquiv Value of the "httpEquiv" attribute, has to be lower case.
 * @return Value of the "content" attribute of the meta tag.
 */
function getMetaContentByHttpEquiv(httpEquiv: string): string {
  for (const metaTag of document.getElementsByTagName('meta')) {
    if (metaTag.httpEquiv.toLowerCase() === httpEquiv) {
      return metaTag.content;
    }
  }
  return '';
}

// Used by the `getTextContent` function below.
const NON_TEXT_NODE_NAMES = new Set([
  'EMBED',
  'NOSCRIPT',
  'OBJECT',
  'SCRIPT',
  'STYLE',
]);

/**
 * Walks a DOM tree to extract the text content.
 * Does not walk into a node when its name is in `NON_TEXT_NODE_NAMES`.
 * @param node The DOM tree
 * @param maxLen Output will be truncated to `maxLen`
 * @return The text content
 */
function getTextContent(node: ChildNode, maxLen: number): string {
  if (!node || maxLen <= 0) {
    return '';
  }

  let txt = '';
  // Formatting and filtering.
  if (node.nodeType === Node.ELEMENT_NODE && node instanceof Element) {
    // Reject non-text nodes such as scripts.
    if (NON_TEXT_NODE_NAMES.has(node.nodeName)) {
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
    for (const childNode of node.childNodes) {
      txt += getTextContent(childNode, maxLen - txt.length);
      if (txt.length >= maxLen) {
        break;
      }
    }
  } else if (node.nodeType === Node.TEXT_NODE && node.textContent) {
    txt += node.textContent.substring(0, maxLen - txt.length);
  }

  return txt;
}

/**
 * Detects if a page has content that needs translation and informs the native
 * side. The text content of a page is cached in `bufferedTextContent` and
 * retrieved at a later time directly from the Obj-C side. This is to avoid
 * sending it back via async messaging.
 */
function detectLanguage(): void {
  // Constant for the maximum length of the extracted text returned by
  // `detectLanguage` to the native side.
  // Matches desktop implementation.
  // Note: This should stay in sync with the constant in
  // ios_language_detection_tab_helper.mm .
  const kMaxIndexChars = 65535;

  activeRequests += 1;
  bufferedTextContent = getTextContent(document.body, kMaxIndexChars);
  const httpContentLanguage = getMetaContentByHttpEquiv('content-language');
  const textCapturedCommand = {
    'hasNoTranslate': false,
    'htmlLang': document.documentElement.lang,
    'httpContentLanguage': httpContentLanguage,
    'frameId': gCrWeb.message.getFrameId(),
  };

  if (hasNoTranslate()) {
    textCapturedCommand['hasNoTranslate'] = true;
  }

  sendWebKitMessage('LanguageDetectionTextCaptured', textCapturedCommand);
}

/**
 * Retrieves the cached text content of a page. Returns it and then purges the
 * cache.
 */
function retrieveBufferedTextContent(): string|null {
  const textContent = bufferedTextContent;
  activeRequests -= 1;
  if (activeRequests === 0) {
    bufferedTextContent = null;
  }
  return textContent;
}

// Mark: Public API

gCrWeb.languageDetection = {
  detectLanguage,
  retrieveBufferedTextContent,
};
