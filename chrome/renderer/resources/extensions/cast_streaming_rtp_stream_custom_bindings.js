// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Cast Streaming RtpStream API.

var natives = requireNative('cast_streaming_natives');

apiBridge.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('destroy',
      function(transportId) {
        natives.DestroyCastRtpStream(transportId);
  });
  apiFunctions.setHandleRequest('getSupportedParams',
      function(transportId) {
        return natives.GetSupportedParamsCastRtpStream(transportId);
  });
  apiFunctions.setHandleRequest('start',
      function(transportId, params) {
        natives.StartCastRtpStream(transportId, params);
  });
  apiFunctions.setHandleRequest('stop',
      function(transportId) {
        natives.StopCastRtpStream(transportId);
  });
  apiFunctions.setHandleRequest('toggleLogging',
      function(transportId, enable) {
        natives.ToggleLogging(transportId, enable);
  });
  apiFunctions.setHandleRequest('getRawEvents',
      function(transportId, extraData, callback) {
        natives.GetRawEvents(transportId, extraData, callback);
  });
  apiFunctions.setHandleRequest('getStats',
      function(transportId, callback) {
        natives.GetStats(transportId, callback);
  });
});
