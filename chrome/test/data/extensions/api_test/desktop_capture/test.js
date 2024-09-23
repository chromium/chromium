// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onPickerResult(audio_track_num, id, options) {
  chrome.test.assertEq("string", typeof id);
  chrome.test.assertNe("", id);
  var video_constraint = { mandatory: { chromeMediaSource: "desktop",
                                        chromeMediaSourceId: id } };
  var audio_constraint =
    options.canRequestAudioTrack ? video_constraint : false;

  navigator.mediaDevices.getUserMedia({
    audio: audio_constraint,
    video: video_constraint
  }).then(
    function(stream) {
      if (audio_track_num != null)
        chrome.test.assertEq(audio_track_num, stream.getAudioTracks().length);
      chrome.test.succeed();
  }).catch(chrome.test.fail);
}

// We can support audio for screen share on Windows. For ChromeOS, it depends
// on whether USE_CRAS is on or not, thus we disable the check here. We cannot
// support audio on other platforms.
var expected_audio_tracks_for_screen_share = 0;
if (navigator.appVersion.indexOf("Windows") != -1)
  expected_audio_tracks_for_screen_share = 1;
else if (navigator.appVersion.indexOf("CrOS") != -1)
  expected_audio_tracks_for_screen_share = null;

chrome.test.runTests([
  function emptySourceList() {
    chrome.desktopCapture.chooseDesktopMedia(
        [],
        chrome.test.callback(function(id) {
          chrome.test.assertEq("undefined", typeof id);
        }, "At least one source type must be specified."));
  },

  // The prompt is canceled.
  function pickerUiCanceled() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq("string", typeof id);
          chrome.test.assertTrue(id == "");
        }));
  },

  // A source is chosen.
  function chooseMedia() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq("string", typeof id);
          chrome.test.assertNe("", id);
        }));
  },

  // For the following four tests FakeDestkopPickerFactory will verify that
  // the right set of sources is selected when creating picker model.
  function screensOnly() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen"], chrome.test.callbackPass(function(id) {}));
  },

  function windowsOnly() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["window"], chrome.test.callbackPass(function(id) {}));
  },

  function tabOnly() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["tab"],
        chrome.test.callbackPass(function(id) {}));
  },

  function audioShareNoApproval() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window", "tab", "audio"],
        chrome.test.callbackPass(function(id, options) {
          chrome.test.assertEq(false, options.canRequestAudioTrack);
        }));
  },

  function audioShareApproval() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window", "tab", "audio"],
        chrome.test.callbackPass(function(id, options) {
          chrome.test.assertEq(true, options.canRequestAudioTrack);
        }));
  },

  // Show window picker and then get the selected stream using
  // getUserMedia().
  function chooseMediaAndGetStream() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"], onPickerResult.bind(undefined, 0));
  },

  // Same as above but attempts to specify invalid source id.
  function chooseMediaAndTryGetStreamWithInvalidId() {
    function onPickerResult(id) {
      navigator.webkitGetUserMedia({
        audio: false,
        video: { mandatory: { chromeMediaSource: "desktop",
                              chromeMediaSourceId: id + "x" } }
      }, chrome.test.fail, chrome.test.succeed);
    }

    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"], onPickerResult);
  },

  function cancelDialog() {
    var requestId = chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        chrome.test.fail);
    chrome.test.assertEq("number", typeof requestId);
    chrome.desktopCapture.cancelChooseDesktopMedia(requestId);
    chrome.test.succeed();
  },

  // For the following six, they all request audio track. Based on user's
  // permission and the source type, it may or may not actually get the audio
  // track.
  // In detail:
  //  1. We cannot support audio for Window share;
  //  2. We can support audio for Tab share;
  //  3. We can support audio for Screen share on Windows;
  //  4. We can support audio for Screen Share on ChromeOS if USE_CRAS is on;
  //  5. To actually get audio track, user permission is always necessary;
  //  6. To actually get audio track, getUserMedia() should set audio
  //     constraint.

  // TODO(crbug.com/41366624): Test fails; invalid device IDs being generated.
  // function tabShareWithAudioPermissionGetStream() {
  //   chrome.desktopCapture.chooseDesktopMedia(
  //       ["tab", "audio"], onPickerResult.bind(undefined, 1));
  // },

  function windowShareWithAudioPermissionGetStream() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["window", "audio"], onPickerResult.bind(undefined, 0));
  },

  function screenShareWithAudioPermissionGetStream() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "audio"],
        onPickerResult.bind(undefined,
                            expected_audio_tracks_for_screen_share));
  },

  // TODO(crbug.com/41366624): Test fails; invalid device IDs being generated.
  // function tabShareWithoutAudioPermissionGetStream() {
  //   chrome.desktopCapture.chooseDesktopMedia(
  //       ["tab", "audio"], onPickerResult.bind(undefined, 0));
  // },

  function windowShareWithoutAudioPermissionGetStream() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["window", "audio"], onPickerResult.bind(undefined, 0));
  },

  function screenShareWithoutAudioPermissionGetStream() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "audio"], onPickerResult.bind(undefined, 0));
  }
]);
