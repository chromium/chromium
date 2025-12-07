// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <controlledframe> Element.

const ControlledFrameImpl = require('controlledFrameImpl').ControlledFrameImpl;
const forwardApiMethods =
    require('guestViewContainerElement').forwardApiMethods;
const upgradeMethodsToPromises =
    require('guestViewContainerElement').upgradeMethodsToPromises;
const ChromeWebViewImpl = require('chromeWebView').ChromeWebViewImpl;
const CONTROLLED_FRAME_API_METHODS =
    require('controlledFrameApiMethods').CONTROLLED_FRAME_API_METHODS;
const CONTROLLED_FRAME_DELETED_API_METHODS =
    require('controlledFrameApiMethods').CONTROLLED_FRAME_DELETED_API_METHODS;
const CONTROLLED_FRAME_PROMISE_API_METHODS =
    require('controlledFrameApiMethods').CONTROLLED_FRAME_PROMISE_API_METHODS;
const convertURLPatternsToMatchPatterns =
    require('controlledFrameURLPatternsHelper')
        .convertURLPatternsToMatchPatterns;
const registerElement = require('guestViewContainerElement').registerElement;
const WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;
const WebViewElement = require('webViewElement').WebViewElement;
const WebViewInternal = getInternalApi('webViewInternal');

// The Web naming conventions for enums are kebab-case while the WebView naming
// conventions which are snake case. Convert from the kebab case convention to
// the snake case convention.
function convertRunAt(webRunAt) {
  if (['document_start', 'document_end', 'document_idle'].includes(webRunAt)) {
    throw new Error(
        'Encountered incorrect naming, please see specification ' +
        'text for correct naming.');
  }

  if (webRunAt === 'document-start') {
    return 'document_start';
  } else if (webRunAt === 'document-end') {
    return 'document_end';
  } else if (webRunAt === 'document-idle') {
    return 'document_idle';
  }

  return webRunAt;
}

// This is a helper function for |convertFromWebNaming()| which receives a
// pre-filled |webViewRule| along with an array of |keyMappings|. Each mapping
// is used to convert a key in |webViewRule| from the Web naming convention to
// the WebView naming convention.
function convertContentScriptDetailsKeys(webViewRule, keyMappings) {
  for (const mapping of keyMappings) {
    if (!('from' in mapping)) {
      throw new Error('\'from\' is required');
    }

    if (!('to' in mapping)) {
      throw new Error('\'to\' is required');
    }

    if (mapping.to in webViewRule) {
      throw new Error(
          'Encountered incorrect naming, please see specification ' +
          'text for correct naming.');
    }

    if (mapping.from in webViewRule) {
      webViewRule[mapping.to] = webViewRule[mapping.from];

      // We assume that |webViewRule| was prefilled from |webViewRule|.
      // Delete the old key from |webViewRule|.
      delete webViewRule[mapping.from];
    }
  }

  return webViewRule;
}

// The Web naming conventions for JavaScript keys are camel case while the
// WebView naming conventions are snake case. Convert from the camel case
// convention to the snake case convention.
function convertFromWebNaming(webRules) {
  const webViewRules = [];
  for (const webRule of webRules) {
    // Convert the "runAt" key if present in each |webRule|.
    if ('runAt' in webRule) {
      webRule.runAt = convertRunAt(webRule.runAt);
    }

    // Prefill |webViewRule| based on |webRule|.
    let webViewRule = webRule;

    // Remove webview fields we don't support.
    delete webViewRule['include_globs'];
    delete webViewRule['exclude_globs'];

    // Convert the keys in |webViewRule|.
    webViewRule = convertContentScriptDetailsKeys(webViewRule, [
      {from: 'allFrames', to: 'all_frames'},
      {from: 'excludeURLPatterns', to: 'exclude_matches'},
      {from: 'matchAboutBlank', to: 'match_about_blank'},
      {from: 'runAt', to: 'run_at'},
      {from: 'urlPatterns', to: 'matches'},
    ]);

    // Convert URLPatterns to match patterns.
    webViewRule.matches =
        convertURLPatternsToMatchPatterns(webViewRule.matches);
    webViewRule.exclude_matches =
        convertURLPatternsToMatchPatterns(webViewRule.exclude_matches);

    webViewRules.push(webViewRule);
  }

  return webViewRules;
}

class HTMLControlledFrameElement extends WebViewElement {
  static get observedAttributes() {
    return WebViewAttributeNames;
  }

  constructor() {
    super();
    privates(this).internal = new ControlledFrameImpl(this);
    privates(this).originalGo = originalGo;
  }

  // Override add/removeContentScripts to accept a `callback` parameter
  // so they can be used with Promises. The upgradeMethodsToPromises call
  // below will replace these with Promise-based versions.
  addContentScripts(rules, callback) {
    const internal = privates(this).internal;
    // Controlled Frame uses a slightly different API convention in its |rules|
    // that follow https://w3ctag.github.io/design-principles/#casing-rules.
    // Adjust incoming calls to translate from the web-preferred model to the
    // previous convention that WebView uses.
    // Note: any incoming rules that continue to use the WebView API will
    // trigger a synchronous failure in calls to this API.
    const webViewRules = convertFromWebNaming(rules);
    return WebViewInternal.addContentScripts(
        internal.viewInstanceId, webViewRules, callback);
  }

  removeContentScripts(names, callback) {
    const internal = privates(this).internal;
    return WebViewInternal.removeContentScripts(
        internal.viewInstanceId, names, callback);
  }

  canGoBack() {
    return $Promise.resolve(super.canGoBack());
  }

  canGoForward() {
    return $Promise.resolve(super.canGoForward());
  }
}

// Forward remaining HTMLControlledFrameElement.foo* method calls to
// ChromeWebViewImpl.foo* or WebViewInternal.foo*.
forwardApiMethods(
    HTMLControlledFrameElement, ControlledFrameImpl, WebViewInternal,
    CONTROLLED_FRAME_API_METHODS, CONTROLLED_FRAME_PROMISE_API_METHODS);

// Since |back| and |forward| are implemented in terms of |go|, we need to
// keep a reference to the real |go| function, since user code may override
// HTMLControlledFrameElement.prototype.go|.
const originalGo = HTMLControlledFrameElement.prototype.go;

// Wrap callback methods in promise handlers. Note: This disables the callback
// forms.
upgradeMethodsToPromises(
    HTMLControlledFrameElement, ControlledFrameImpl, WebViewInternal,
    CONTROLLED_FRAME_PROMISE_API_METHODS);

// Delete GuestView methods that should not be part of the Controlled Frame API.
(function() {
for (const methodName of CONTROLLED_FRAME_DELETED_API_METHODS) {
  let clazz = HTMLControlledFrameElement.prototype;
  while ((methodName in clazz) && clazz.constructor.name !== 'HTMLElement') {
    delete clazz[methodName];
    clazz = $Object.getPrototypeOf(clazz);
  }
}
})();

registerElement(
    'ControlledFrame', 'HTMLControlledFrameElement',
    HTMLControlledFrameElement);
