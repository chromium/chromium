// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common js for prerender loaders; defines the helper functions that put
// event handlers on prerenders and track the events for browser tests.

// TODO(gavinp): Put more common loader logic in here.

function AddPrerender(url, index) {
  var link = document.createElement('link');
  link.id = 'prerenderElement' + index;
  link.rel = 'prerender';
  link.href = url;
  document.body.appendChild(link);
  return link;
}

function RemoveLinkElement(index) {
  var link = document.getElementById('prerenderElement' + index);
  link.parentElement.removeChild(link);
}

function ExtractGetParameterBadlyAndInsecurely(param, defaultValue) {
  var re = RegExp('[&?]' + param + '=([^&?#]*)');
  var result = re.exec(document.location);
  if (result)
    return result[1];
  return defaultValue;
}

function AddAnchor(href, target) {
  var a = document.createElement('a');
  a.href = href;
  if (target)
    a.target = target;
  document.body.appendChild(a);
  return a;
}

function Click(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1
  }));
}

function ClickTarget(url) {
  var eventObject = new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1
  });
  AddAnchor(url, '_blank').dispatchEvent(eventObject);
}

function ClickPing(url, pingUrl) {
  var a = AddAnchor(url);
  a.ping = pingUrl;
  a.dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1
  }));
}

function ShiftClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    shiftKey: true
  }));
}

function CtrlClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    ctrlKey: true
  }));
}

function CtrlShiftClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    ctrlKey: true,
    shiftKey: true
  }));
}

function MetaClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    metaKey: true
  }));
}

function MetaShiftClick(url) {
  AddAnchor(url).dispatchEvent(new MouseEvent('click', {
    view: window,
    bubbles: true,
    cancelable: true,
    detail: 1,
    metaKey: true,
    shiftKey: true
  }));
}

function WindowOpen(url) {
  window.open(url);
}
