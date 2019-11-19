// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <webview> API.
// See web_view_api_methods.js for details.

var ChromeWebView = getInternalApi('chromeWebViewInternal');
var ChromeWebViewSchema =
    requireNative('schema_registry').GetSchema('chromeWebViewInternal');
var CreateEvent = require('guestViewEvents').CreateEvent;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var idGeneratorNatives = requireNative('id_generator');
var registerElement = require('guestViewContainerElement').registerElement;
var utils = require('utils');
var WebViewElement = require('webViewElement').WebViewElement;
var WebViewImpl = require('webView').WebViewImpl;
var WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;

// This is the only "webViewInternal.onClicked" named event for this renderer.
//
// Since we need an event per <webview>, we define events with suffix
// (subEventName) in each of the <webview>. Behind the scenes, this event is
// registered as a ContextMenusEvent, with filter set to the webview's
// |viewInstanceId|. Any time a ContextMenusEvent is dispatched, we re-dispatch
// it to the subEvent's listeners. This way
// <webview>.contextMenus.onClicked behave as a regular chrome Event type.
var ContextMenusEvent = CreateEvent('chromeWebViewInternal.onClicked');
// See comment above.
var ContextMenusHandlerEvent =
    CreateEvent('chromeWebViewInternal.onContextMenuShow');

function GetUniqueSubEventName(eventName) {
  return eventName + '/' + idGeneratorNatives.GetNextId();
}

// This event is exposed as <webview>.contextMenus.onClicked.
function createContextMenusOnClickedEvent(webViewInstanceId,
                                          opt_eventName,
                                          opt_argSchemas,
                                          opt_eventOptions) {
  var subEventName = GetUniqueSubEventName(opt_eventName);
  var newEvent =
      bindingUtil.createCustomEvent(subEventName, false, false);

  var view = GuestViewInternalNatives.GetViewFromID(webViewInstanceId);
  if (view) {
    view.events.addScopedListener(
        ContextMenusEvent,
        $Function.bind(function() {
          // Re-dispatch to subEvent's listeners.
          $Function.apply(newEvent.dispatch, newEvent, $Array.slice(arguments));
        }, newEvent),
        {instanceId: webViewInstanceId});
  }
  return newEvent;
}

// This event is exposed as <webview>.contextMenus.onShow.
function createContextMenusOnContextMenuEvent(webViewInstanceId,
                                              opt_eventName,
                                              opt_argSchemas,
                                              opt_eventOptions) {
  var subEventName = GetUniqueSubEventName(opt_eventName);
  var newEvent =
      bindingUtil.createCustomEvent(subEventName, false, false);

  var view = GuestViewInternalNatives.GetViewFromID(webViewInstanceId);
  if (view) {
    view.events.addScopedListener(
        ContextMenusHandlerEvent,
        $Function.bind(function(e) {
          var defaultPrevented = false;
          var event = {
            preventDefault: function() { defaultPrevented = true; }
          };

          // Re-dispatch to subEvent's listeners.
          $Function.apply(newEvent.dispatch, newEvent, [event]);

          if (!defaultPrevented) {
          // TODO(lazyboy): Remove |items| parameter completely from
          // ChromeWebView.showContextMenu as we don't do anything useful with
          // it currently.
          var items = [];
          var guestInstanceId = GuestViewInternalNatives.
              GetViewFromID(webViewInstanceId).guest.getId();
          ChromeWebView.showContextMenu(guestInstanceId, e.requestId, items);
        }
      }, newEvent),
      {instanceId: webViewInstanceId});
  }

  return newEvent;
}

// -----------------------------------------------------------------------------
// WebViewContextMenusImpl object.

// An instance of this class is exposed as <webview>.contextMenus.
function WebViewContextMenusImpl(viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
}
$Object.setPrototypeOf(WebViewContextMenusImpl.prototype, null);

WebViewContextMenusImpl.prototype.create = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusCreate, null, args);
};

WebViewContextMenusImpl.prototype.remove = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusRemove, null, args);
};

WebViewContextMenusImpl.prototype.removeAll = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusRemoveAll, null, args);
};

WebViewContextMenusImpl.prototype.update = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusUpdate, null, args);
};

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
});

// -----------------------------------------------------------------------------

class ChromeWebViewImpl extends WebViewImpl {
  constructor(webviewElement) {
    super(webviewElement);
    this.setupContextMenus();
  }
}

ChromeWebViewImpl.prototype.setupContextMenus = function() {
  if (!this.contextMenusOnContextMenuEvent_) {
    var eventName = 'chromeWebViewInternal.onContextMenuShow';
    var eventSchema =
        utils.lookup(ChromeWebViewSchema.events, 'name', 'onShow');
    var eventOptions = {supportsListeners: true, supportsLazyListeners: false};
    this.contextMenusOnContextMenuEvent_ = createContextMenusOnContextMenuEvent(
        this.viewInstanceId, eventName, eventSchema, eventOptions);
  }

  var createContextMenus = $Function.bind(function() {
    return this.weakWrapper(function() {
      if (this.contextMenus_) {
        return this.contextMenus_;
      }

      this.contextMenus_ = new WebViewContextMenus(this.viewInstanceId);

      // Define 'onClicked' event property on |this.contextMenus_|.
      var getOnClickedEvent = $Function.bind(function() {
        return this.weakWrapper(function() {
          if (!this.contextMenusOnClickedEvent_) {
            var eventName = 'chromeWebViewInternal.onClicked';
            var eventSchema =
                utils.lookup(ChromeWebViewSchema.events, 'name', 'onClicked');
            var eventOptions =
                {supportsListeners: true, supportsLazyListeners: false};
            var onClickedEvent = createContextMenusOnClickedEvent(
                this.viewInstanceId, eventName, eventSchema, eventOptions);
            this.contextMenusOnClickedEvent_ = onClickedEvent;
            return onClickedEvent;
          }
          return this.contextMenusOnClickedEvent_;
        });
      }, this);
      $Object.defineProperty(
          this.contextMenus_,
          'onClicked',
          {get: getOnClickedEvent(), enumerable: true});
      $Object.defineProperty(
          this.contextMenus_,
          'onShow',
          {
            get: this.weakWrapper(function() {
              return this.contextMenusOnContextMenuEvent_;
            }),
            enumerable: true
          });
      return this.contextMenus_;
    });
  }, this);

  // Expose <webview>.contextMenus object.
  $Object.defineProperty(
      this.element,
      'contextMenus',
      {
        get: createContextMenus(),
        enumerable: true
      });
};

class ChromeWebViewElement extends WebViewElement {
  static get observedAttributes() {
    return WebViewAttributeNames;
  }

  constructor() {
    super();
    privates(this).internal = new ChromeWebViewImpl(this);
  }
}

registerElement('WebView', ChromeWebViewElement);
