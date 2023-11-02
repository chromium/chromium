// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onmessage = function(e) {
  var respond = function(message) {
    e.ports[0].postMessage(message);
  };

  switch (e.data) {
    case 'checknotification':
      var permission = Notification.permission;
      respond(permission == 'granted' ?
          'OK' : ('Unexpected Notification.permission: ' + permission));
      break;
    case 'shownotification':
      var result = registration.showNotification(
          'Hello title.', {body: 'Hello body.'});
      e.waitUntil(result.then(function() {
        respond('OK');
      }, function(err) {
        respond('showNotification failed.');
      }));
      break;
    default:
      respond('Received unexpected message: ' + e.data);
      break;
  }
};
