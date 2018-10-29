// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

var mockController;

function setUp() {
  mockController = new MockController();
  installMockXMLHttpRequest();
}

function tearDown() {
  mockController.verifyMocks();
  mockController.reset();
  uninstallMockXMLHttpRequest();
  mockSyncFS.reset();
  mockLocalFS.reset();
}

// Test set custom wallpaper from syncFS. When local wallpaper name is different
// with the name in server, wallpaper should use the one in server.
function testSyncCustomWallpaperSet() {
  var mockSetCustomWallpaper = mockController.createFunctionMock(
      chrome.wallpaperPrivate, 'setCustomWallpaper');
  mockSetCustomWallpaper.addExpectation(
      TestConstants.FILESTRING, 'dummy', true /*generateThumbnail=*/,
      TestConstants.wallpaperUrl, false /*previewMode=*/);
  var syncFSChanges = {};
  syncFSChanges.status = 'synced';
  syncFSChanges.direction = 'remote_to_local';
  syncFSChanges.action = 'added';
  syncFSChanges.fileEntry = mockSyncFS.mockTestFile(TestConstants.wallpaperUrl);
  chrome.syncFileSystem.onFileStatusChanged.dispatch(syncFSChanges);
}

// Test store historical custom wallpaper. When receive a historical wallpaper
// from syncFS, we store it to local.
function testSyncCustomWallpaperStore() {
  var syncFSChanges = {};
  syncFSChanges.status = 'synced';
  syncFSChanges.direction = 'remote_to_local';
  syncFSChanges.action = 'added';
  syncFSChanges.fileEntry = mockSyncFS.mockTestFile('historicalwallpaper');

  // TODO(ranj): support two callbacks with success and failure?
  var mockWrite = mockController.createFunctionMock(mockWriter, 'write');
  mockWrite.addExpectation(new Blob([new Int8Array(TestConstants.FILESTRING)]));
  chrome.syncFileSystem.onFileStatusChanged.dispatch(syncFSChanges);
}

// Test that historical custom wallpapers from syncFS get synced to localFS on
// powerwashed (or recovered) devices.
function testSyncCustomWallpaperStoreOnPowerwashedDevice() {
  TestConstants.isPowerwashed = 1;
  var syncFSChanges = {};
  syncFSChanges.status = 'synced';
  syncFSChanges.direction = 'remote_to_local';
  syncFSChanges.action = 'added';
  syncFSChanges.fileEntry = mockSyncFS.mockTestFile('historicalwallpaper');

  // TODO(ranj): support two callbacks with success and failure?
  var mockWrite = mockController.createFunctionMock(mockWriter, 'write');
  mockWrite.addExpectation(new Blob([new Int8Array(TestConstants.FILESTRING)]));
  chrome.syncFileSystem.onFileStatusChanged.dispatch(syncFSChanges);
  TestConstants.isPowerwashed = 0;
}


// Test delete custom wallpaper from local. When receive a syncFS delete file
// event, delete the file in localFS as well.
function testSyncCustomWallpaperDelete() {
  var localOriginalPath = Constants.WallpaperDirNameEnum.ORIGINAL + '/' +
                          'deletewallpapername';
  var localThumbnailPath = Constants.WallpaperDirNameEnum.THUMBNAIL + '/' +
                           'deletewallpapername';
  var originalFE = mockLocalFS.mockTestFile(localOriginalPath);
  var thumbnailFE = mockLocalFS.mockTestFile(localThumbnailPath);
  var syncFSChanges = {};
  syncFSChanges.status = 'synced';
  syncFSChanges.direction = 'remote_to_local';
  syncFSChanges.action = 'deleted';
  syncFSChanges.fileEntry = new FileEntry('deletewallpapername');
  var mockRemoveOriginal = mockController.createFunctionMock(originalFE,
                                                             'remove');
  mockRemoveOriginal.addExpectation(function() {}, null);
  var mockRemoveThumbnail = mockController.createFunctionMock(thumbnailFE,
                                                              'remove');
  mockRemoveThumbnail.addExpectation(function() {}, null);
  chrome.syncFileSystem.onFileStatusChanged.dispatch(syncFSChanges);
}

// Test sync online wallpaper. When the synced wallpaper info is not the same as
// the local wallpaper info, wallpaper should set to the synced one.
function testSyncOnlineWallpaper() {
  // Setup sync value.
  var changes = {};
  changes[Constants.AccessSyncWallpaperInfoKey] = {};
  changes[Constants.AccessSyncWallpaperInfoKey].newValue = {
    'url': TestConstants.wallpaperUrl,
    'layout': 'custom',
    'source': Constants.WallpaperSourceEnum.Online
  };

  var mockSetWallpaperIfExists = mockController.createFunctionMock(
      chrome.wallpaperPrivate, 'setWallpaperIfExists');
  mockSetWallpaperIfExists.addExpectation(
      changes[Constants.AccessSyncWallpaperInfoKey].newValue.url,
      changes[Constants.AccessSyncWallpaperInfoKey].newValue.layout,
      false /*previewMode=*/);
  mockSetWallpaperIfExists.callbackData = [false];

  var mockSetWallpaper = mockController.createFunctionMock(
      chrome.wallpaperPrivate, 'setWallpaper');
  mockSetWallpaper.addExpectation(
      TestConstants.IMAGE,
      changes[Constants.AccessSyncWallpaperInfoKey].newValue.layout,
      changes[Constants.AccessSyncWallpaperInfoKey].newValue.url,
      false /*previewMode=*/);

  chrome.storage.onChanged.dispatch(changes);
}

// Test the surprise wallpaper's UMA stats is recorded correctly.
function testSurpriseWallpaper() {
  var mockSetWallpaperIfExists = mockController.createFunctionMock(
      chrome.wallpaperPrivate, 'setWallpaperIfExists');
  mockSetWallpaperIfExists.addExpectation(
      TestConstants.wallpaperUrl + TestConstants.highResolutionSuffix,
      'CENTER_CROPPED', false /*previewMode=*/);
  mockSetWallpaperIfExists.callbackData = [true];

  var mockRecordWallpaperUMA = mockController.createFunctionMock(
      chrome.wallpaperPrivate, 'recordWallpaperUMA');
  mockRecordWallpaperUMA.addExpectation(Constants.WallpaperSourceEnum.Daily);

  var dateString = new Date().toDateString();
  SurpriseWallpaper.getInstance().setRandomWallpaper_(dateString);
}
