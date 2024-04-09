// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script is run multiple times with different configurations. The content
// scripts need to determine synchronously which subtest is being run. This
// state is communicated via the URL.
var TEST_HOST = location.search.slice(1);
chrome.test.assertTrue(
    TEST_HOST !== '',
    'The subtest type must be specified in the query string.');

var config;
var tabId;

function getTestUrl(page) {
  return 'http://' + TEST_HOST + ':' + config.testServer.port +
      '/extensions/api_test/executescript/destructive/' + page;
}

chrome.test.getConfig(function(config) {
  window.config = config;

  chrome.tabs.create({url: 'about:blank'}, function(tab) {
    tabId = tab.id;
    startTest();
  });
});

function startTest() {
  // These tests load a page that contains a frame, where a content script will
  // be run (and remove the frame). The injection point depends on the URL:
  // - ?blankstart = Load a script in the blank frame at document_start.
  // - ?blankend = Load a script in the blank frame at document_end.
  // - ?start = Load a script in the non-blank frame at document_start.
  // - ?end = Load a script in the non-blank frame at document_end.

  var kEmptyHtmlBodyPattern = /<body><\/body>/;

  var allTests = [
    // Empty document.
    function removeHttpFrameAtDocumentStart() {
      testRemoveSelf('empty_frame.html?start');
    },

    function removeHttpFrameAtDocumentEnd() {
      testRemoveSelf('empty_frame.html?end', kEmptyHtmlBodyPattern);
    },

    // about:blank
    function removeAboutBlankAtDocumentStart() {
      testRemoveSelf('about_blank_frame.html?blankstart');
    },

    function removeAboutBlankAtDocumentEnd() {
      testRemoveSelf('about_blank_frame.html?blankend', kEmptyHtmlBodyPattern);
    },

    // srcdoc frame with a HTML tag
    function removeAboutSrcdocHtmlAtDocumentStart() {
      testRemoveSelf('srcdoc_html_frame.html?blankstart');
    },

    function removeAboutSrcdocHtmlAtDocumentEnd() {
      testRemoveSelf('srcdoc_html_frame.html?blankend', /<br>/);
    },

    // srcdoc frame with text
    function removeAboutSrcdocTextOnlyAtDocumentStart() {
      testRemoveSelf('srcdoc_text_frame.html?blankstart');
    },

    function removeAboutSrcdocTextOnlyAtDocumentEnd() {
      testRemoveSelf('srcdoc_text_frame.html?blankend', /text/);
    },

    // <iframe></iframe> (no URL)
    function removeFrameWithoutUrlAtDocumentStart() {
      testRemoveSelf('no_url_frame.html?blankstart');
    },

    function removeFrameWithoutUrlAtDocumentEnd() {
      testRemoveSelf('no_url_frame.html?blankend', kEmptyHtmlBodyPattern);
    },

    // An image.
    function removeImageAtDocumentStart() {
      testRemoveSelf('image_frame.html?start');
    },

    function removeImageAtDocumentEnd() {
      testRemoveSelf('image_frame.html?end', /<img/);
    },

    // Audio (media).
    function removeAudioAtDocumentStart() {
      testRemoveSelf('audio_frame.html?start');
    },

    function removeAudioAtDocumentEnd() {
      testRemoveSelf('audio_frame.html?end', /<video.+ type="audio\//);
    },

    // Video (media).
    function removeVideoAtDocumentStart() {
      testRemoveSelf('video_frame.html?start');
    },

    function removeVideoAtDocumentEnd() {
      testRemoveSelf('video_frame.html?end', /<video.+ type="video\//);
    },

    // Plugins
    function removePluginAtDocumentStart() {
      if (maybeSkipPluginTest())
        return;
      testRemoveSelf('plugin_frame.html?start');
    },

    function removePluginAtDocumentEnd() {
      if (maybeSkipPluginTest())
        return;
      testRemoveSelf(
          'plugin_frame.html?end', /background-color: rgb\(82, 86, 89\)/);
    },

    // Plain text
    function removePlainTextAtDocumentStart() {
      testRemoveSelf('txt_frame.html?start');
    },

    function removePlainTextAtDocumentEnd() {
      testRemoveSelf('txt_frame.html?end', /<pre/);
    },

    // XHTML
    function removeXhtmlAtDocumentStart() {
      testRemoveSelf(
          'xhtml_frame.html?start',
          /<html xmlns="http:\/\/www\.w3\.org\/1999\/xhtml"><\/html>/);
    },

    function removeXhtmlAtDocumentEnd() {
      testRemoveSelf(
          'xhtml_frame.html?end',
          /<html xmlns="http:\/\/www\.w3\.org\/1999\/xhtml">/);
    },

    // XML
    function removeXmlAtDocumentStart() {
      testRemoveSelf('xml_frame.html?start', /<root\/>/);
    },

    function removeXmlAtDocumentEnd() {
      testRemoveSelf('xml_frame.html?end', /<root><child\/><\/root>/);
    },
  ];

  // Parameters defined in execute_script_apitest.cc.
  var testParams = new URLSearchParams(location.hash.slice(1));
  var kBucketCount = parseInt(testParams.get('bucketcount'));
  var kBucketIndex = parseInt(testParams.get('bucketindex'));
  var kTestsPerBucket = Math.ceil(allTests.length / kBucketCount);

  chrome.test.assertTrue(kBucketCount * kTestsPerBucket >= allTests.length,
      'To cover all tests, the number of buckets multiplied by the number of ' +
      'tests per bucket must be at least as big as the number of tests.');
  chrome.test.assertTrue(0 <= kBucketIndex >= 0 && kBucketIndex < kBucketCount,
      'There are only ' + kBucketCount + ' buckets, so the bucket index must ' +
      'be between 0 and ' + kBucketCount + ', but it was ' + kBucketIndex);

  // Every run except for the last run contains |kTestsPerBucket| tests. The
  // last run (i.e. |kBucketIndex| = |kBucketCount| - 1) may contain fewer tests
  // if there are not enough remaining tests, since the total number of tests is
  // not necessarily divisible by |kTestsPerBucket|.
  var filteredTests =
    allTests.slice(kBucketIndex * kTestsPerBucket).slice(0, kTestsPerBucket);

  // At the document_end stage, the parser will not modify the DOM any more, so
  // we can skip those tests that wait for DOM mutations to save time.
  if (TEST_HOST.startsWith('dom')) {
    filteredTests = filteredTests.filter(function(testfn) {
      // Omit the *Documentend tests = keep the *DocumentStart tests.
      return !testfn.name.endsWith('DocumentEnd');
    });
  }
  chrome.test.runTests(filteredTests);
}

// |page| must be an existing page in this directory, and the URL must be
// matched by one content script. Otherwise the test will time out.
//
// If the regular expression |pattern| is specified, then the serialization of
// the frame content must match the pattern. This ensures that the tests are
// still testing the expected document in the future.
function testRemoveSelf(page, pattern) {
  // By default, the serialization of the document must be non-empty.
  var kDefaultPattern = /</;

  if (page.includes('start')) {
    pattern = pattern || /^<\s*html[^>]*><\/html>$/;
    pattern = TEST_HOST === 'synchronous' ? pattern : kDefaultPattern;
  } else if (page.includes('end')) {
    pattern = pattern || kDefaultPattern;
  } else {
    chrome.test.fail('URL must contain "start" or "end": ' + page);
  }

  chrome.test.listenOnce(chrome.runtime.onMessage, function(msg, sender) {
    chrome.test.assertEq(tabId, sender.tab && sender.tab.id);
    chrome.test.assertEq(0, sender.frameId);

    var frameHTML = msg.frameHTML;
    delete msg.frameHTML;
    chrome.test.assertEq({frameCount: 0}, msg);

    chrome.test.assertTrue(
        pattern.test(frameHTML),
        'The pattern ' + pattern + ' should be matched by: ' + frameHTML);
  });
  chrome.tabs.update(tabId, {url: getTestUrl(page)});
}

// The plugin test requires a plugin to be installed. Skip the test if plugins
// are not supported.
function maybeSkipPluginTest() {
  // This MIME-type should be handled by a browser plugin.
  var kPluginMimeType = 'application/pdf';
  for (var i = 0; i < navigator.plugins.length; ++i) {
    var plugin = navigator.plugins[i];
    for (var j = 0; j < plugin.length; ++j) {
      var mimeType = plugin[j];
      if (mimeType.type === kPluginMimeType)
        return false;
    }
  }
  var kMessage = 'Plugin not found for ' + kPluginMimeType + ', skipping test.';
  console.log(kMessage);
  chrome.test.log(kMessage);
  chrome.test.succeed();
  return true;
}
