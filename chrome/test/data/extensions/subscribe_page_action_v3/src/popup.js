// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function feedLink(url) {
  var feed_link = document.createElement('a');
  feed_link.href = url;
  feed_link.addEventListener("click", onClick);
  return feed_link;
}

function main() {
  chrome.tabs.query({ active: true, currentWindow: true }, function(tabs) {
    const tab = tabs[0];
    chrome.storage.local.get(tab.id.toString(), function(result) {
      let input = result[tab.id];
      let feeds = input !== undefined ? input['feeds'] : undefined;
      // There are pages where our scripts can't run, such as chrome:// pages,
      // so we must abort if the current url doesn't match the url associated
      // with the feed list (i.e. if the user navigates to a feed page and then
      // to about:version, the feed from the previous page should not show).
      if (feeds === undefined || input['url'] !== tab.url) {
        var msg = document.getElementById('heading');
        msg.innerText =
            chrome.i18n.getMessage("rss_subscription_not_valid_feed");
      } else if (feeds.length == 1) {
        // Only one feed, no need for a bubble; go straight to the subscribe
        // page.
        preview(feeds[0].href);
      } else {
        var content = document.getElementById('content');
        var heading = document.getElementById('heading');
        heading.innerText =
            chrome.i18n.getMessage("rss_subscription_action_title");
        content.appendChild(document.createElement('br'));

        var feed_list = document.createElement('table');
        feed_list.style.width = "400";
        for (var i = 0; i < feeds.length; ++i) {
          // Create an RSS image and the anhor encapsulating it.
          var img_link = feedLink(feeds[i].href);
          var img = document.createElement('img');
          img.src = "feed-icon-16x16.png";
          img_link.appendChild(img);

          // Create a text node and the anchor encapsulating it.
          var text_link = feedLink(feeds[i].href);
          text_link.appendChild(document.createTextNode(feeds[i].title));

          // Add the data to a row in the table.
          var tr = document.createElement('tr');
          tr.className = "feedList";
          var td = document.createElement('td');
          td.width = "16";
          td.appendChild(img_link);
          var td2 = document.createElement('td');
          td2.appendChild(text_link);
          tr.appendChild(td);
          tr.appendChild(td2);
          feed_list.appendChild(tr);
        }

        content.appendChild(feed_list);
      }
    });
  });
}

function onClick(event) {
  var a = event.currentTarget;
  preview(a.href);
}

function preview(feed_url) {
  // See if we need to skip the preview page and subscribe directly.
  var url = "";
  if (window.localStorage && window.localStorage.showPreviewPage == "No") {
    // Skip the preview.
    url = window.localStorage.defaultReader.replace("%s", escape(feed_url));
  } else {
    // Show the preview page.
    url = "subscribe.html?" + encodeURIComponent(feed_url);
  }
  chrome.tabs.create({ url: url });
  window.close();
}

// Init on DOM ready.
document.addEventListener('DOMContentLoaded', main);
