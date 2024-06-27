// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Grab the querystring, removing question mark at the front and splitting on
// the ampersand.
var queryString = location.search.substring(1).split("&");

// The feed URL is the first component and always present.
var feedUrl = decodeURIComponent(queryString[0]);

// This extension's ID.
var extension_id = chrome.i18n.getMessage("@@extension_id");

// The XMLHttpRequest object that tries to load and parse the feed, and (if
// testing) also the style sheet and the frame js.
var req;

// Depending on whether this is run from a test or from the extension, this
// will either be a link to the css file within the extension or contain the
// contents of the style sheet, fetched through XmlHttpRequest.
var styleSheet = "";

// Depending on whether this is run from a test or from the extension, this
// will either be a link to the js file within the extension or contain the
// contents of the style sheet, fetched through XmlHttpRequest.
var frameScript = "";

// What to show when we cannot parse the feed name.
var unknownName = chrome.i18n.getMessage("rss_subscription_unknown_feed_name");

// A list of feed readers, populated by localStorage if available, otherwise
// hard coded.
var feedReaderList;

// The token to use during communications with the iframe.
var token = "";

// Navigates to the reader of the user's choice (for subscribing to the feed).
function navigate() {
  var select = document.getElementById('readerDropdown');
  var url =
      feedReaderList[select.selectedIndex].url.replace(
          "%s", encodeURIComponent(feedUrl));

  // Before we navigate, see if we want to skip this step in the future...
  if (storageEnabled) {
    // See if the user wants to always use this reader.
    var alwaysUse = document.getElementById('alwaysUse');
    if (alwaysUse.checked) {
      window.localStorage.defaultReader =
          feedReaderList[select.selectedIndex].url;
      window.localStorage.showPreviewPage = "No";
    }
  }

  document.location = url;
}

/**
* The main function. Sets up the selection list for possible readers and
* fetches the data.
*/
function main() {
  if (storageEnabled && window.localStorage.readerList)
      feedReaderList = JSON.parse(window.localStorage.readerList);
  if (!feedReaderList)
    feedReaderList = defaultReaderList();

  // Populate the list of readers.
  var readerDropdown = document.getElementById('readerDropdown');
  for (i = 0; i < feedReaderList.length; ++i) {
    readerDropdown.options[i] = new Option(feedReaderList[i].description, i);
    if (storageEnabled && isDefaultReader(feedReaderList[i].url))
      readerDropdown.selectedIndex = i;
  }

  if (storageEnabled) {
    // Add the "Manage..." entry to the dropdown and show the checkbox asking
    // if we always want to use this reader in the future (skip the preview).
    readerDropdown.options[i] =
        new Option(chrome.i18n.getMessage("rss_subscription_manage_label"), "");
    document.getElementById('alwaysUseSpan').style.display = "block";
  }

  // Set the token.
  var tokenArray  = new Uint32Array(4);
  crypto.getRandomValues(tokenArray);
  token = [].join.call(tokenArray);

  styleSheet = "<link rel='stylesheet' type='text/css' href='" +
                   chrome.runtime.getURL("style.css") + "'>";
  frameScript = window.domAutomationController !== undefined ? "<script src='" +
                    chrome.runtime.getURL("test_support.js") +
                    "'></" + "script>" : "";
  frameScript += "<script src='" + chrome.runtime.getURL("iframe.js") +
                     "'></" + "script>";

  // Now fetch the feed data.
  req = new XMLHttpRequest();
  req.onload = handleResponse;
  req.onerror = handleError;
  req.open("GET", feedUrl, true);
  // Not everyone sets the mime type correctly, which causes handleResponse
  // to fail to XML parse the response text from the server. By forcing
  // it to text/xml we avoid this.
  req.overrideMimeType('text/xml');
  req.send(null);

  document.getElementById('feedUrl').href = 'view-source:' + feedUrl;
}

// Sets the title for the feed.
function setFeedTitle(title) {
  var titleTag = document.getElementById('title');
  titleTag.textContent =
      chrome.i18n.getMessage("rss_subscription_feed_for", title);
}

// Handles errors during the XMLHttpRequest.
function handleError() {
  handleFeedParsingFailed(
      chrome.i18n.getMessage("rss_subscription_error_fetching"));
}

// Handles feed parsing errors.
function handleFeedParsingFailed(error) {
  setFeedTitle(unknownName);

  // The tests always expect an IFRAME, so add one showing the error.
  var html = "<body><span id=\"error\" class=\"item_desc\">" + error +
               "</span></body>";
  if (window.domAutomationController) {
    html += "<script src='" + chrome.runtime.getURL("test_send_error.js") +
                     "'></" + "script>";
  }

  var error_frame = createFrame('error', html);
  var itemsTag = document.getElementById('items');
  itemsTag.appendChild(error_frame);
}

function createFrame(frame_id, html) {
  var csp = '<meta http-equiv="content-security-policy" ' +
          'content="object-src \'none\'; script-src \'self\'">';
  frame = document.createElement('iframe');
  frame.id = frame_id;
  frame.name = "preview";
  frame.sandbox = "allow-scripts";
  frame.src = "data:text/html;charset=utf-8,<html>" + csp +
              "<!--Token:" + extension_id + token +
              "-->" + html + "</html>";
  frame.scrolling = "auto";
  frame.frameBorder = "0";
  frame.marginWidth = "0";
  return frame;
}

// Handles parsing the feed data we got back from XMLHttpRequest.
function handleResponse() {
  // Uncomment these three lines to see what the feed data looks like.
  // var itemsTag = document.getElementById('items');
  // itemsTag.textContent = req.responseText;
  // return;

  var doc = req.responseXML;
  if (!doc) {
    // If the XMLHttpRequest object fails to parse the feed we make an attempt
    // ourselves, because sometimes feeds have html/script code appended below a
    // valid feed, which makes the feed invalid as a whole even though it is
    // still parsable.
    var domParser = new DOMParser();
    doc = domParser.parseFromString(req.responseText, "text/xml");
    if (!doc) {
      handleFeedParsingFailed(
          chrome.i18n.getMessage("rss_subscription_not_valid_feed"));
      return;
    }
  }

  // We must find at least one 'entry' or 'item' element before proceeding.
  var entries = doc.getElementsByTagName('entry');
  if (entries.length == 0)
    entries = doc.getElementsByTagName('item');
  if (entries.length == 0) {
    handleFeedParsingFailed(
        chrome.i18n.getMessage("rss_subscription_no_entries"))
    return;
  }

  // Figure out what the title of the whole feed is.
  var title = doc.getElementsByTagName('title')[0];
  if (title)
    setFeedTitle(title.textContent);
  else
    setFeedTitle(unknownName);

  // Embed the iframe.
  var itemsTag = document.getElementById('items');
  // TODO(aa): Add base URL tag
  iframe = createFrame('rss', styleSheet + frameScript);
  itemsTag.appendChild(iframe);
}

/**
* Handler for when selection changes.
*/
function onSelectChanged() {
  if (!storageEnabled)
    return;
  var readerDropdown = document.getElementById('readerDropdown');

  // If the last item (Manage...) was selected we show the options.
  var oldSelection = readerDropdown.selectedIndex;
  if (readerDropdown.selectedIndex == readerDropdown.length - 1)
    window.location = "options.html";
}

document.addEventListener('DOMContentLoaded', function () {
  document.title =
      chrome.i18n.getMessage("rss_subscription_default_title");
  i18nReplace('rss_subscription_subscribe_using');
  i18nReplace('rss_subscription_subscribe_button');
  i18nReplace('rss_subscription_always_use');
  i18nReplace('rss_subscription_feed_preview');
  i18nReplaceImpl('feedUrl', 'rss_subscription_feed_link', '');

  var dropdown = document.getElementById('readerDropdown');
  dropdown.addEventListener('change', onSelectChanged);
  var button = document.getElementById('rss_subscription_subscribe_button');
  button.addEventListener('click', navigate);

  main();
});

window.addEventListener("message", function(e) {
  if (e.ports[0] && e.data === token)
    e.ports[0].postMessage(req.responseText);
}, false);
