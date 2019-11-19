// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Restores the interstitial content to the initial state if the window size
 * switches to a small view.
 */
function onResize() {
  var mediaQuery = '(max-height:11.25em) and (max-width:18.75em),' +
       '(max-height:18.75em) and (max-width:11.25em),' +
       '(max-height:5em), (max-width:5em)';

  // Check for change in window size.
  if (window.matchMedia(mediaQuery).matches) {
    var hiddenDetails = $('details').classList.add(HIDDEN_CLASS);
    $('main-content').classList.remove(HIDDEN_CLASS);
  }
}

function initPage() {
  var isGiantWebView = loadTimeData.getBoolean('is_giant');
  var darkModeAvailable = loadTimeData.getBoolean('darkModeAvailable');
  var interstitialType = loadTimeData.getString('type');
  var safebrowsing = interstitialType == "SAFEBROWSING";
  var heavyAd = interstitialType == "HEAVYAD";

  document.body.className = isGiantWebView ? 'giant' : '';

  if (darkModeAvailable) {
    document.body.classList.add('dark-mode-available');
  }

  if (heavyAd) {
    document.body.classList.add('heavy-ad');
  }

  preventDefaultOnPoundLinkClicks();

  $('details-link').addEventListener('click', function(event) {
    var hiddenDetails = $('details').classList.toggle(HIDDEN_CLASS);
    $('main-content').classList.toggle(HIDDEN_CLASS, !hiddenDetails);
  });

  if (safebrowsing) {
    $('proceed-link').addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
    });
  }

  window.addEventListener('resize', onResize);
}

document.addEventListener('DOMContentLoaded', initPage);
