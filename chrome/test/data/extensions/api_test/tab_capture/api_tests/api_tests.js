// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabCapture = chrome.tabCapture;

var helloWorldPageUri = 'data:text/html;charset=UTF-8,' +
    encodeURIComponent('<html><body>Hello world!</body></html>');

function assertIsSameSetOfTabs(list_a, list_b, id_field_name) {
  chrome.test.assertEq(list_a.length, list_b.length);
  function tabIdSortFunction(a, b) {
    return (a[id_field_name] || 0) - (b[id_field_name] || 0);
  }
  list_a.sort(tabIdSortFunction);
  list_b.sort(tabIdSortFunction);
  for (var i = 0, end = list_a.length; i < end; ++i) {
    chrome.test.assertEq(list_a[i][id_field_name], list_b[i][id_field_name]);
  }
}

// Since all of these tests run in the same tab with limited teardown between
// tests, we have to be extra-careful to not pollute state between tests. Tests
// should be completely sure that capture state is as expected before exiting,
// otherwise the next test in the suite will fail. For context, see
// https://crbug.com/764464 for some of the motivation here.
const CaptureState = {
  kPending: 'pending',
  kActive: 'active',
  kStopped: 'stopped'
};
let g_state = CaptureState.kStopped;
let g_should_call_succeed = false;

tabCapture.onStatusChanged.addListener(function(info) {
  g_state = info.status;
  if (g_should_call_succeed) {
    chrome.test.succeed();
    g_should_call_succeed = false;
  }
});

// Since if capture is already stopped this method immediately calls succeed,
// tests should use it directly in place of a chrome.test.succeed call.
function succeedOnCaptureStopped() {
  if (g_state == CaptureState.kStopped) {
    chrome.test.succeed();
  } else {
    g_should_call_succeed = true;
  }
}

function assertIsValidStreamId(streamId) {
  chrome.test.assertTrue(typeof streamId == 'string');
  navigator.webkitGetUserMedia({
    audio: false,
    video: {
      mandatory: {
        chromeMediaSource: 'tab',
        chromeMediaSourceId: streamId
      }
    }
  }, function(stream) {
    chrome.test.assertTrue(!!stream);
    stream.getVideoTracks()[0].stop();
    succeedOnCaptureStopped();
  }, function(error) {
    chrome.test.fail(error);
  });
}

function assertGetUserMediaError(streamId) {
  chrome.test.assertTrue(typeof streamId == 'string');
  navigator.webkitGetUserMedia(
      {
        audio: false,
        video: {
          mandatory: {chromeMediaSource: 'tab', chromeMediaSourceId: streamId}
        }
      },
      function(stream) {
        chrome.test.assertTrue(!!stream);
        stream.getVideoTracks()[0].stop();
        chrome.test.fail('Should not get stream.');
      },
      function(error) {
        succeedOnCaptureStopped();
      });
}

var testsToRun = [
  function captureTabAndVerifyStateTransitions() {
    // Tab capture events in the order they happen.
    var tabCaptureEvents = [];

    var tabCaptureListener = function(info) {
      if (info.status == 'stopped') {
        chrome.test.assertEq('active', tabCaptureEvents.pop());
        chrome.test.assertEq('pending', tabCaptureEvents.pop());
        tabCapture.onStatusChanged.removeListener(tabCaptureListener);
        chrome.test.succeed();
        return;
      }
      tabCaptureEvents.push(info.status);
    };
    tabCapture.onStatusChanged.addListener(tabCaptureListener);

    tabCapture.capture({audio: true, video: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.getVideoTracks()[0].stop();
      stream.getAudioTracks()[0].stop();
    });
  },

  function getCapturedTabs() {
    chrome.tabs.create({active: true}, function(secondTab) {
      // chrome.tabCapture.capture() will only capture the active tab.
      chrome.test.assertTrue(secondTab.active);

      function checkInfoForSecondTabHasStatus(infos, status) {
        for (var i = 0; i < infos.length; ++i) {
          if (infos[i].tabId == secondTab) {
            chrome.test.assertNe(null, status);
            chrome.test.assertEq(status, infos[i].status);
            chrome.test.assertEq(false, infos[i].fullscreen);
            return;
          }
        }
      }

      // Step 4: After the second tab is closed, check that getCapturedTabs()
      // returns no info at all about the second tab.
      chrome.tabs.onRemoved.addListener(function() {
        tabCapture.getCapturedTabs(function checkNoInfos(infos) {
          checkInfoForSecondTabHasStatus(infos, null);
          chrome.test.succeed();
        });
      });

      var activeStream = null;

      // Step 3: After the stream is stopped, check that getCapturedTabs()
      // returns 'stopped' capturing status for the second tab.
      var capturedTabsAfterStopCapture = function(infos) {
        checkInfoForSecondTabHasStatus(infos, 'stopped');
        chrome.tabs.remove(secondTab.id);
      };

      // Step 2: After the stream is started, check that getCapturedTabs()
      // returns 'active' capturing status for the second tab.
      var capturedTabsAfterStartCapture = function(infos) {
        checkInfoForSecondTabHasStatus(infos, 'active');
        activeStream.getVideoTracks()[0].stop();
        activeStream.getAudioTracks()[0].stop();
        tabCapture.getCapturedTabs(capturedTabsAfterStopCapture);
      };

      // Step 1: Start capturing the second tab (the currently active tab).
      tabCapture.capture({audio: true, video: true}, function(stream) {
        chrome.test.assertTrue(!!stream);
        activeStream = stream;
        tabCapture.getCapturedTabs(capturedTabsAfterStartCapture);
      });
    });
  },

  function captureSameTab() {
    var stream1 = null;

    var tabMediaRequestCallback2 = function(stream) {
      chrome.test.assertLastError(
          'Cannot capture a tab with an active stream.');
      chrome.test.assertTrue(!stream);
      stream1.getVideoTracks()[0].stop();
      stream1.getAudioTracks()[0].stop();
      succeedOnCaptureStopped();
    };

    tabCapture.capture({audio: true, video: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream1 = stream;
      tabCapture.capture({audio: true, video: true}, tabMediaRequestCallback2);
    });
  },

  function onlyVideo() {
    tabCapture.capture({video: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.getVideoTracks()[0].stop();
      succeedOnCaptureStopped();
    });
  },

  function onlyAudio() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.getAudioTracks()[0].stop();
      succeedOnCaptureStopped();
    });
  },

  function noAudioOrVideoRequested() {
    // If not specified, video is not requested.
    tabCapture.capture({audio: false}, function(stream) {
      chrome.test.assertLastError(
          'Capture failed. No audio or video requested.');
      chrome.test.assertTrue(!stream);
      succeedOnCaptureStopped();
    });
  },

  function getMediaStreamIdWithCallerTab() {
    chrome.tabs.getCurrent(function(tab) {
      tabCapture.getMediaStreamId({consumerTabId: tab.id}, function(streamId) {
        assertIsValidStreamId(streamId);
      });
    });
  },

  function getMediaStreamIdWithTargetTab() {
    chrome.tabs.getCurrent(function(tab) {
      tabCapture.getMediaStreamId({targetTabId: tab.id}, function(streamId) {
        assertIsValidStreamId(streamId);
      });
    });
  },

  function getMediaStreamIdWithoutTabIds() {
    tabCapture.getMediaStreamId(function(streamId) {
      assertIsValidStreamId(streamId);
    });
  },

  // Test that if calling getMediaStreamId() with consumer tab specified and
  // then calling getUserMedia() on another tab will fail to get the stream.
  function getMediaStreamIdAndGetUserMediaOnDifferentTabs() {
    var currentTabId;
    var secondTabId;

    var listener = function(tabId, changeInfo, tab) {
      if (changeInfo.status == 'complete') {
        chrome.tabs.onUpdated.removeListener(listener);
        tabCapture.getMediaStreamId(
            {consumerTabId: secondTabId},
            function(streamId) {
              chrome.tabs.get(currentTabId, function(tab) {
                assertGetUserMediaError(streamId);
                chrome.tabs.remove(secondTabId);
              });
            });
      }
    };

    chrome.tabs.onUpdated.addListener(listener);
    chrome.tabs.getCurrent(function(tab) {
      currentTabId = tab.id;
      chrome.tabs.create({}, function(tab) {
        secondTabId = tab.id;
      });
    });
  },

  function getMediaStreamIdWithCallerTabWithoutSecuredUrl() {
    var listener = function(tabId, changeInfo, tab) {
      if (changeInfo.status == 'complete') {
        chrome.tabs.onUpdated.removeListener(listener);
        tabCapture.getMediaStreamId({consumerTabId: tabId}, function(streamId) {
          chrome.test.assertTrue(typeof streamId == 'undefined');
          chrome.test.assertLastError(
              'URL scheme for the specified tab is not secure.');
          chrome.test.succeed();
          chrome.tabs.remove(tab.id);
        });
      }
    };

    chrome.tabs.onUpdated.addListener(listener);
    chrome.tabs.create({url: 'http://example.com/fun.html'});
  },
];

chrome.test.runTests(testsToRun);
