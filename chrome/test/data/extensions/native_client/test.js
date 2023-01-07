// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedded_event;
var content_handler_event;

function setListeners(body_element) {
    var eventListener = function(e) {
        var target_element = e.target;
        if (target_element.className != 'naclModule')
            return;

        var element_id = target_element.id;
        if (element_id == 'embedded')
            embedded_event = e.type;
        else if (element_id == 'content_handler')
            content_handler_event = e.type;
    }
    body_element.addEventListener('loadstart', eventListener, true);
    body_element.addEventListener('error', eventListener, true);
}

function EmbeddedPluginCreated() {
    return embedded_event != undefined;
}

function ContentHandlerPluginCreated() {
    return content_handler_event != undefined;
}
