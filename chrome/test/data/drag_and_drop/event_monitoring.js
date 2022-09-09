// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Reports an event to DragAndDropBrowserTest and  DOMDragEventWaiter */
window.reportDragAndDropEvent = function(ev) {
  function safe(f) {
    try {
      return f();
    } catch(err) {
      return "Got exception: " + err.message;
    }
  }

  var msg = "Got a " + ev.type + " event from the " + window.name + " frame.";
  console.log(msg);

  if (window.domAutomationController) {
    window.domAutomationController.send({
      client_position: safe(function() {
        return "(" + ev.clientX + ", " + ev.clientY + ")";
      }),
      drop_effect: safe(function() { return ev.dataTransfer.dropEffect; }),
      effect_allowed: safe(function() {
        return ev.dataTransfer.effectAllowed;
      }),
      event_type: ev.type,
      file_names: safe(function() {
        return Array
            .from(ev.dataTransfer.files)
            .map(function(file) { return file.name; })
            .sort().join();
      }),
      mime_types: safe(function() {
        return Array.from(ev.dataTransfer.types).sort().join();
      }),
      page_position: safe(function() {
        return "(" + ev.pageX + ", " + ev.pageY + ")";
      }),
      screen_position: safe(function() {
        return "(" + ev.screenX + ", " + ev.screenY + ")";
      }),
      window_name: window.name
    });
  }
}
