// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

let gasm_streams;
let gdm_streams;

let gasm_ended_promise;
let gdm_ended_promise;

function verifyMethod(method) {
  return (method == "getAllScreensMedia" || method == "getDisplayMedia");
}

async function run(method) {
  if (!verifyMethod(method)) {
    throw new Error("Unknown method");
  }

  if (method == "getAllScreensMedia") {
    gasm_streams = await navigator.mediaDevices.getAllScreensMedia();
    gasm_ended_promise = createStopPromisesForStreams(gasm_streams);
  } else {  // "getDisplayMedia"
    gdm_streams = await navigator.mediaDevices.getDisplayMedia();
    gdm_ended_promise = createStopPromisesForStreams([gdm_streams]);
  }
}

async function stop(method) {
  if (!verifyMethod(method)) {
    throw new Error("Unknown method");
  }

  const streams =
      (method == "getAllScreensMedia") ? gasm_streams : [gdm_streams];
  streams.forEach(stream => {
    stream.getTracks().forEach(track => {
      track.stop();
    });
  });
}

async function runGetAllScreensMediaAndGetIds() {
  try {
    gasm_streams = await navigator.mediaDevices.getAllScreensMedia();
    const stream_ids = gasm_streams.map(stream => stream.id).toString();
    const track_ids =
      gasm_streams.map(stream => stream.getTracks()[0].id).toString();
    return logAndReturn(stream_ids + ":" + track_ids);
  } catch (error) {
    return logAndReturn("capture-failure," + error.name);
  }
}

function videoTrackContainsScreenDetailed(track_id) {
  const stream = gasm_streams.find(
    stream => stream.getTracks()[0].id === track_id
  );
  if (!stream) {
    return logAndReturn("error-stream-not-found");
  }

  const video_tracks = stream.getVideoTracks();
  if (video_tracks.length != 1) {
    return logAndReturn("error-invalid-tracks-size");
  }

  const video_track = video_tracks[0];
  if (typeof video_track.screenDetailed !== "function") {
    return logAndReturn("error-no-screen-detailed");
  }

  try {
    const screen_detailed = video_track.screenDetailed();
    if (!screen_detailed) {
      return logAndReturn("error-screen-detailed-does-not-exist");
    }
    return logAndReturn("success-screen-detailed");
  } catch (error) {
    return logAndReturn("error-screen-detailed");
  }
}

function createStopPromisesForStreams(streams) {
  const promises = [];
  streams.forEach(stream => {
    stream.getTracks().forEach((track) => {
      promises.push(
        new Promise(resolve => {
          track.onended = () => { resolve(); };
        })
      );
    });
  });

  return Promise.all(promises);
}


async function waitUntilStoppedByUser(method) {
  if (!verifyMethod(method)) {
    throw new Error("Unknown method");
  }

  await (method == "getAllScreensMedia") ? gasm_ended_promise :
    gdm_ended_promise;
}

function areAllTracksLive(method) {
  if (!verifyMethod(method)) {
    throw new Error("Unknown method");
  }
  const streams =
    (method == "getAllScreensMedia") ? gasm_streams : [gdm_streams];
  return streams.every(stream =>
    stream.getTracks().every(track => track.readyState == "live"));
}