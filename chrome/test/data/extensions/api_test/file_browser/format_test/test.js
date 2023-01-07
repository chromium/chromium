// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const expectedError = 'Error in invocation of fileManagerPrivate.' +
    'formatVolume(string volumeId, fileManagerPrivate.FormatFileSystemType ' +
    'filesystem, string volumeLabel): Error at parameter \'filesystem\': ' +
    'Value must be one of exfat, ntfs, vfat.';

chrome.test.runTests([
  function formatVolumes() {
    const filesystemType = chrome.fileManagerPrivate.FormatFileSystemType;
    chrome.fileManagerPrivate.formatVolume(
        'removable:mount_path1', filesystemType.VFAT, 'NEWLABEL1');
    chrome.fileManagerPrivate.formatVolume(
        'removable:mount_path2', filesystemType.EXFAT, 'NEWLABEL2');
    chrome.fileManagerPrivate.formatVolume(
        'removable:mount_path3', filesystemType.NTFS, 'NEWLABEL3');

    // Test an unsupported filesystem.
    chrome.test.assertThrows(
        chrome.fileManagerPrivate.formatVolume,
        ['removable:mount_path3', 'invalid-fs', 'NEWLABEL3'], expectedError);

    // This test is also checked on the C++ side, which tests that
    // disk_mount_manager.FormatMountedVolume() gets called exactly once for
    // each corresponding formatVolume() call.
    chrome.test.succeed();
  },
]);
