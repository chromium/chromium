// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <webview> API.
// See web_view_api_methods.js for details.

const ChromeWebView = getInternalApi('chromeWebViewInternal');
const ChromeWebViewSchema =
    requireNative('schema_registry').GetSchema('chromeWebViewInternal');
const CreateEvent = require('guestViewEvents').CreateEvent;
const GuestViewInternalNatives = requireNative('guest_view_internal');
const idGeneratorNatives = requireNative('id_generator');
const utils = require('utils');
const WebViewImpl = require('webView').WebViewImpl;

// This is the only "webViewInternal.onClicked" named event for this renderer.
//
// Since we need an event per <webview>, we define events with suffix
// (subEventName) in each of the <webview>. Behind the scenes, this event is
// registered as a ContextMenusEvent, with filter set to the webview's
// |viewInstanceId|. Any time a ContextMenusEvent is dispatched, we re-dispatch
// it to the subEvent's listeners. This way
// <webview>.contextMenus.onClicked behave as a regular chrome Event type.
const ContextMenusEvent = CreateEvent('chromeWebViewInternal.onClicked');
// See comment above.
const ContextMenusHandlerEvent =
    CreateEvent('chromeWebViewInternal.onContextMenuShow');

function GetUniqueSubEventName(eventName) {
  return eventName + '/' + idGeneratorNatives.GetNextId();
}

// This event is exposed as <webview>.contextMenus.onClicked.
function createContextMenusOnClickedEvent(
    webViewInstanceId, opt_eventName, opt_argSchemas, opt_eventOptions) {
  const subEventName = GetUniqueSubEventName(opt_eventName);
  const newEvent = bindingUtil.createCustomEvent(subEventName, false, false);

  const view = GuestViewInternalNatives.GetViewFromID(webViewInstanceId);
  if (view) {
    view.events.addScopedListener(ContextMenusEvent, $Function.bind(function() {
      // Re-dispatch to subEvent's listeners.
      $Function.apply(newEvent.dispatch, newEvent, $Array.slice(arguments));
    }, newEvent), {instanceId: webViewInstanceId});
  }
  return newEvent;
}

// This event is exposed as <webview>.contextMenus.onShow.
function createContextMenusOnContextMenuEvent(
    webViewInstanceId, opt_eventName, opt_argSchemas, opt_eventOptions) {
  const subEventName = GetUniqueSubEventName(opt_eventName);
  const newEvent = bindingUtil.createCustomEvent(subEventName, false, false);

  const view = GuestViewInternalNatives.GetViewFromID(webViewInstanceId);
  if (view) {
    view.events.addScopedListener(
        ContextMenusHandlerEvent, $Function.bind(function(e) {
          let defaultPrevented = false;
          const event = {
            preventDefault: function() {
              defaultPrevented = true;
            },
          };

          // Re-dispatch to subEvent's listeners.
          $Function.apply(newEvent.dispatch, newEvent, [event]);

          if (!defaultPrevented) {
            const guestInstanceId =
                GuestViewInternalNatives.GetViewFromID(webViewInstanceId)
                    .guest.getId();
            //TODO: Remove the unused items array from the definition of
            //showContextMenu.
            ChromeWebView.showContextMenu(guestInstanceId, e.requestId);
          }
        }, newEvent), {instanceId: webViewInstanceId});
  }

  return newEvent;
}

// -----------------------------------------------------------------------------
// WebViewContextMenusImpl object.

// An instance of this class is exposed as <webview>.contextMenus.
function WebViewContextMenusImpl(webView, viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
  this.setupEvents(webView);
}
$Object.setPrototypeOf(WebViewContextMenusImpl.prototype, null);

WebViewContextMenusImpl.prototype.create = function() {
  const args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusCreate, null, args);
};

WebViewContextMenusImpl.prototype.remove = function() {
  const args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusRemove, null, args);
};

WebViewContextMenusImpl.prototype.removeAll = function() {
  const args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusRemoveAll, null, args);
};

WebViewContextMenusImpl.prototype.update = function() {
  const args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusUpdate, null, args);
};

WebViewContextMenusImpl.prototype.setupEvents = function(webView) {
  // Define 'onClicked' event property on |contextMenus|.
  const getOnClickedEvent = $Function.bind(function() {
    return webView.weakWrapper(function() {
      if (!webView.contextMenusOnClickedEvent_) {
        const eventName = 'chromeWebViewInternal.onClicked';
        const eventSchema =
            utils.lookup(ChromeWebViewSchema.events, 'name', 'onClicked');
        const eventOptions = {
          supportsListeners: true,
          supportsLazyListeners: false,
        };
        const onClickedEvent = createContextMenusOnClickedEvent(
            webView.viewInstanceId, eventName, eventSchema, eventOptions);
        webView.contextMenusOnClickedEvent_ = onClickedEvent;
        return onClickedEvent;
      }
      return webView.contextMenusOnClickedEvent_;
    });
  }, webView);

  $Object.defineProperty(
      this, 'onClicked', {get: getOnClickedEvent(), enumerable: true});
  $Object.defineProperty(this, 'onShow', {
    get: webView.weakWrapper(function() {
      return webView.contextMenusOnContextMenuEvent_;
    }),
    enumerable: true,
  });
};

// Arguments: webView, viewInstanceId
function WebViewContextMenus() {
  privates(WebViewContextMenus).constructPrivate(this, arguments);
}

utils.expose(WebViewContextMenus, WebViewContextMenusImpl, {
  functions: [
    'create',
    'remove',
    'removeAll',
    'update',
  ],
  readonly: ['onClicked', 'onShow'],
});

// -----------------------------------------------------------------------------

class ChromeWebViewImpl extends WebViewImpl {
  constructor(webviewElement) {
    super(webviewElement);
    this.setupContextMenus();
  }
}

ChromeWebViewImpl.prototype.createWebViewContextMenus = function() {
  return new WebViewContextMenus(this, this.viewInstanceId);
};

ChromeWebViewImpl.prototype.setupContextMenus = function() {
  // Code below can not be member of a context menus instance,
  // belongs to webview implementations, crbug.com/429599984
  if (!this.contextMenusOnContextMenuEvent_) {
    const eventName = 'chromeWebViewInternal.onContextMenuShow';
    const eventSchema =
        utils.lookup(ChromeWebViewSchema.events, 'name', 'onShow');
    const eventOptions = {
      supportsListeners: true,
      supportsLazyListeners: false,
    };
    this.contextMenusOnContextMenuEvent_ = createContextMenusOnContextMenuEvent(
        this.viewInstanceId, eventName, eventSchema, eventOptions);
  }

  const createContextMenus = $Function.bind(function() {
    return this.weakWrapper(function() {
      if (this.contextMenus_) {
        return this.contextMenus_;
      }

      this.contextMenus_ = this.createWebViewContextMenus();
      return this.contextMenus_;
    });
  }, this);

  // Expose <webview>.contextMenus object.
  $Object.defineProperty(
      this.element, 'contextMenus',
      {get: createContextMenus(), enumerable: true});
};

exports.$set('WebViewContextMenusImpl', WebViewContextMenusImpl);
exports.$set('ChromeWebViewImpl', ChromeWebViewImpl);
