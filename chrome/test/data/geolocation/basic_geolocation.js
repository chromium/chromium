// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var last_position = 0;
var last_error = 0;
var watch_id = 0;
var position_initialized = false;
var position_updated = false;

const callbackStatuses = new ResultQueue();
const geopositionUpdates = new ResultQueue();

function resetState() {
  last_position = 0;
  last_error = 0;
  watch_id = 0;
  position_initialized = false;
  position_updated = false;
}
function geoSuccessCallback(position) {
  last_position = position;
}
function geoErrorCallback(error) {
  last_error = error;
}
function geoSuccessCallbackWithResponse(position) {
  last_position = position;

  callbackStatuses.push('request-callback-success');
  if (!position_initialized) {  // First time callback invoked.
    position_initialized = true;
    return;
  }

  if (!position_updated) {  // Second time callback invoked.
    position_updated = true;
    geopositionUpdates.push('geoposition-updated');
  }
}
function geoErrorCallbackWithResponse(error) {
  last_error = error;
  callbackStatuses.push('request-callback-error');
}
function geoStart() {
  resetState();
  watch_id = navigator.geolocation.watchPosition(
      geoSuccessCallback, geoErrorCallback,
      {maximumAge:600000, timeout:100000, enableHighAccuracy:true});
  return watch_id;
}
function geoStartWithAsyncResponse() {
  resetState();
  navigator.geolocation.watchPosition(
    geoSuccessCallbackWithResponse, geoErrorCallbackWithResponse,
      {maximumAge:600000, timeout:100000, enableHighAccuracy:true});
  return callbackStatuses.pop();
}
function geoStartWithSyncResponse() {
  resetState();
  navigator.geolocation.watchPosition(
            geoSuccessCallback, geoErrorCallback,
            {maximumAge:600000, timeout:100000, enableHighAccuracy:true});
  return 'requested';
}
function geoGetLastPositionLatitude() {
  return "" + last_position.coords.latitude;
}
function geoGetLastPositionLongitude() {
  return "" + last_position.coords.longitude;
}
function geoGetLastError() {
  return "" + (last_error ? last_error.code : 0);
}
function geoAccessNavigatorGeolocation() {
  return "" + typeof(navigator.geolocation);
}
