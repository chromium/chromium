// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function goBackIfPossible() {
  if (history.length > 1) {
    history.go(-1);
    return true;
  }

  return false;
}

async function handlePrepareFeedIcon(request, sender) {
  // First validate that all the URLs have the right schema.
  var input = {
    url: request.url,
    feeds: []
  };
  for (var i = 0; i < request.feeds.length; ++i) {
    const feedUrl = new URL(request.feeds[i].href);
    if (feedUrl.protocol == "http:" || feedUrl.protocol == "https:") {
      input['feeds'].push(request.feeds[i]);
    } else {
      console.log('Warning: feed source rejected (wrong protocol): ' +
                  request.feeds[i].href);
    }
  }

  if (input['feeds'].length == 0) {
    return;  // We've rejected all the input, so abort.
  }

  // We have received a list of feed urls found on the page.
  var storageEntry = {};
  storageEntry[sender.tab.id] = input;
  await chrome.storage.local.set(storageEntry, function() {
    const action_title =
        chrome.i18n.getMessage("rss_subscription_action_title");
    // Enable the page action icon.
    chrome.action.setTitle({ tabId: sender.tab.id, title: action_title });
    chrome.action.setIcon({
      tabId: sender.tab.id,
      path: { "16": "feed-icon-16x16.png" }
    });
  });
}

async function handleFeedDocument(request, sender) {
  // We received word from the content script that this document
  // is an RSS feed (not just a document linking to the feed).
  // So, we go straight to the subscribe page in a new tab and
  // navigate back on the current page (to get out of the xml page).
  // We don't want to navigate in-place because trying to go back
  // from the subscribe page takes us back to the xml page, which
  // will redirect to the subscribe page again (we don't support a
  // location.replace equivalent in the Tab navigation system).

  let attemptCloseTabAfterRedirect = false;
  try {
    const results = await chrome.scripting.executeScript(
        {
          target: { tabId: sender.tab.id },
          func: goBackIfPossible
        });
    let navigatedBack = results[0].result;

    // Calling goBackIfPossible results in three possibilities:
    // - Error: An Exception was thrown, for example if we tried to run
    //          executescript on the NewTab page (see catch handler below).
    // - Success: In which case we do nothing.
    // - Failure: There's apparently no page in history to go back to, which
    //            means the user navigated directly to a feed back. This is the
    //            only time we should attempt to close the page, because it
    //            doesn't make sense to both show the XML code and show the
    //            Subscribe page.
    if (!navigatedBack) attemptCloseTabAfterRedirect = true;
  } catch (exception) {
    console.log('Error calling executeScript', exception);
  }

  var url = "subscribe.html?" + encodeURIComponent(request.href);
  url = chrome.runtime.getURL(url);
  chrome.tabs.create({ url: url, index: sender.tab.index });

  if (attemptCloseTabAfterRedirect) {
    chrome.tabs.remove(sender.tab.id, function() { });
  }
}

chrome.runtime.onMessage.addListener(function(request, sender) {
  if (request.msg == "feedIcon") {
    handlePrepareFeedIcon(request, sender);
  } else if (request.msg == "feedDocument") {
    handleFeedDocument(request, sender);
  }
});

chrome.tabs.onRemoved.addListener(function(tabId) {
  chrome.storage.local.remove(tabId.toString());
});
