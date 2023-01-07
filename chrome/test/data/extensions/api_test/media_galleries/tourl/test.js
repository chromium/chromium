// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function check_result_factory(expected) {
  return function() {
    if (expected) {
      chrome.test.succeed();
    } else {
      chrome.test.fail();
    }
  };
}

function TestImageLoadFactory(url, should_load) {
  return function() {
    test_image = document.createElement('img');
    document.body.appendChild(test_image);
    test_image.addEventListener('load', check_result_factory(should_load));
    test_image.addEventListener('error', check_result_factory(!should_load));

    test_image.src = url;
  };
}

chrome.test.getConfig(function(config) {
  var customArg = JSON.parse(config.customArg);
  var gallery_id = customArg[0];
  var profile_path = customArg[1];
  var extension_url = chrome.runtime.getURL('/');
  var extension_id = extension_url.split('/')[2];

  var bad_mount_point = 'filesystem:' + extension_url +
    'external/invalid-' + profile_path + '-' + extension_id + '-' + gallery_id +
    '/test.jpg';

  var bad_mount_name = 'filesystem:' + extension_url +
    'external/media_galleries-' + profile_path + '-' + gallery_id + '/test.jpg';

  var good_url = 'filesystem:' + extension_url +
    'external/media_galleries-' + profile_path + '-' + extension_id + '-' +
    gallery_id + '/test.jpg';

  chrome.test.runTests([
    TestImageLoadFactory(bad_mount_point, false),
    TestImageLoadFactory(bad_mount_name, false),
    TestImageLoadFactory(good_url, true),
  ]);
})
