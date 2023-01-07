// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var galleries;
var invalidGalleryId = '11000';

// chrome.mediaGalleries.getMediaFileSystems callback.
var mediaFileSystemsListCallback = function (results) {
  galleries = results;
  chrome.test.sendMessage('get_media_file_systems_callback_ok');
};

// Gallery changed event handler.
var onGalleryChangedCallback = function (details) {
  chrome.test.sendMessage('gallery_changed_event_received');
};

// Add watch request callback.
var onAddWatchRequestCallback = function (details) {
  if (details.success) {
    chrome.test.sendMessage('add_watch_request_succeeded');
  } else {
    chrome.test.sendMessage('add_watch_request_failed');
  }
};

var onGalleryChangedCheckingCallback = function(result) {
  if (result.galleryId != '' && result.type == 'contents_changed') {
    chrome.test.sendMessage('on_gallery_changed_checking_ok');
  }
};


/**
 * Generates a callback function which notifies the apitest when the given
 * number of runtime errors has occurred.
 *
 * @param {number} expectedNumCalls The number of calls to this callback to
 *     expect.
 * @return {function()}
 */
var createUnlistenedAddWatchCallback = function(expectedNumCalls) {
  var numCalls = 0;
  var numErrors = 0;
  return function() {
    numCalls++;
    if (chrome.runtime.lastError) {
      numErrors++;
    }

    if (numCalls == expectedNumCalls && numErrors == expectedNumCalls) {
      chrome.test.sendMessage('add_watch_request_runtime_error');
    }
  };
}

// Helpers to add and remove event listeners.
function addGalleryChangedListener() {
  chrome.mediaGalleries.onGalleryChanged.addListener(
      onGalleryChangedCallback);
  chrome.test.sendMessage('add_gallery_changed_listener_ok');
};

function addCheckingGalleryChangedListener() {
  chrome.mediaGalleries.onGalleryChanged.addListener(
      onGalleryChangedCheckingCallback);
  chrome.test.sendMessage('add_gallery_changed_listener_ok');
};

function setupWatchOnValidGalleries() {
  for (var i = 0; i < galleries.length; ++i) {
    var info = chrome.mediaGalleries.getMediaFileSystemMetadata(galleries[i]);
    chrome.mediaGalleries.addGalleryWatch(info.galleryId,
                                          onAddWatchRequestCallback);
  }
  chrome.test.sendMessage('add_gallery_watch_ok');
};

function setupWatchOnUnlistenedValidGalleries() {
  var callback = createUnlistenedAddWatchCallback(galleries.length);
  for (var i = 0; i < galleries.length; ++i) {
    var info = chrome.mediaGalleries.getMediaFileSystemMetadata(galleries[i]);
    chrome.mediaGalleries.addGalleryWatch(info.galleryId, callback);
  }
};

function setupWatchOnInvalidGallery() {
  chrome.mediaGalleries.addGalleryWatch(invalidGalleryId,
                                        onAddWatchRequestCallback);
}

function getMediaFileSystems() {
  chrome.mediaGalleries.getMediaFileSystems(mediaFileSystemsListCallback);
  chrome.test.sendMessage('get_media_file_systems_ok');
};

function removeGalleryWatch() {
  for (var i = 0; i < galleries.length; ++i) {
    var info = chrome.mediaGalleries.getMediaFileSystemMetadata(galleries[i]);
    chrome.mediaGalleries.removeGalleryWatch(info.galleryId);
  }
  chrome.test.sendMessage('remove_gallery_watch_ok');
};

function removeGalleryChangedListener() {
  chrome.mediaGalleries.onGalleryChanged.removeListener(
      onGalleryChangedCallback);
  chrome.test.sendMessage('remove_gallery_changed_listener_ok');
};
