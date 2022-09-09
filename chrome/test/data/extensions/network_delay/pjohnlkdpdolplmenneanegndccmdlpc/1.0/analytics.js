/**
 * Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Below is a modified version of the Google Analytics asynchronous tracking
 * code snippet.  It has been modified to pull the HTTPS version of ga.js
 * instead of the default HTTP version.  It is recommended that you use this
 * snippet instead of the standard tracking snippet provided when setting up
 * a Google Analytics account.
 *
 * See http://code.google.com/apis/analytics/docs/tracking/asyncTracking.html
 * for information on how to use the asynchronous tracking API.
 *
 * If you wish to use this file in your own extension, replace UA-12026369-1
 * with your own Google Analytics account number.  Note that the default code
 * will automatically track a page view for any page this file is included in.
 *
 * When including this file in your code, the best practice is to insert the
 * <script src="analytics.js"></script> include at the top of the <body>
 * section of your pages, after the opening <body> tag.
 */


// Testing note: Accessing external resources is normally a bad idea in a unit
// test. However, the whole point of this test extension is to ensure that
// the resource referenced here does not need to load before the test finishes.
// Otherwise, it will cause a deadlock: see http://crbug.com/107148.
// It should therefore not slow down testing.

var _gaq = _gaq || [];
_gaq.push(['_setAccount', 'UA-12026369-1']);
_gaq.push(['_trackPageview']);

(function() {
  var ga = document.createElement('script'); ga.type = 'text/javascript';
  ga.src = 'https://ssl.google-analytics.com/ga.js';
  var s = document.getElementsByTagName('script')[0];
  s.parentNode.insertBefore(ga, s);
})();
