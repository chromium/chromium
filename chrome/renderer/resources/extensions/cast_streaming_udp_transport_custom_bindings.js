// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Cast Streaming UdpTransport API.

var natives = requireNative('cast_streaming_natives');

apiBridge.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('destroy', function(transportId) {
    natives.DestroyCastUdpTransport(transportId);
  });
  apiFunctions.setHandleRequest('setDestination',
    function(transportId, destination) {
      natives.SetDestinationCastUdpTransport(transportId, destination);
  });
  apiFunctions.setHandleRequest('setOptions',
    function(transportId, options) {
      natives.SetOptionsCastUdpTransport(transportId, options);
  });
});
