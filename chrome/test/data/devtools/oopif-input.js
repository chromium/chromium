// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var logs = [];
function log(text) {
    logs.push(text);
}

function logMouseEvent(event) {
    log('Event');
    log('type: ' + event.type);
    log('button: ' + event.button);
    if (event.shiftKey)
        log('shiftKey');
    log('x: ' + event.x);
    log('y: ' + event.y);
    if (event.type === 'mousewheel') {
        log('deltaX: ' + event.deltaX);
        log('deltaY: ' + event.deltaY);
    }
    event.preventDefault();
}

function logKeyEvent(event) {
    log('Event');
    log('type: ' + event.type);
    event.preventDefault();
}

function logTouchEvent(event) {
    log('Event');
    log('type: ' + event.type);
    for (var touch of event.touches) {
      log('touch x: ' + touch.pageX);
      log('touch y: ' + touch.pageY);
    }
    event.preventDefault();
}

window.addEventListener('mousedown', logMouseEvent);
window.addEventListener('mouseup', logMouseEvent);
window.addEventListener('contextmenu', logMouseEvent);
window.addEventListener('mousewheel', logMouseEvent);
window.addEventListener('keydown', logKeyEvent);
window.addEventListener('touchstart', logTouchEvent);
window.addEventListener('touchend', event => event.preventDefault());
