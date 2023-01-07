// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.management.getAll(function(items) {
  for (var i in items) {
    var item = items[i];
    if (item.name == 'packaged_app') {
      launchFromBackground(item.id);
      break;
    }
  }
});

function launchFromBackground(appId) {
  // Create a new 'popup' window so the last active window isn't 'normal'.
  chrome.windows.create({url: 'about:blank', type: 'popup'}, function(win) {
    chrome.management.launchApp(appId, function() {
      chrome.windows.getAll({populate: true}, function(wins) {
        if (wins.length != 2)
          return;

        // This test passes if the 'popup' window still has only 1 tab,
        // and if the 'normal' window now has 2 tabs. (The app tab was
        // added to the 'normal' window even if it wasn't focused.)
        for (var x = 0; x < wins.length; x++) {
          var w = wins[x];
          if (w.id == win.id) {
            if (w.tabs.length > 1)
              return;
            if (w.tabs[0].url != 'about:blank')
              return;

          } else if (w.type == 'normal') {
            if (w.tabs.length == 2)
              chrome.test.sendMessage('success');
          }
        }
      });
    });
  });
}
