// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('RSSExtension.IFrame');

goog.require('goog.html.sanitizer.HtmlSanitizer');
goog.require('goog.dom.safe');

// The maximum number of feed items to show in the preview.
const maxFeedItems = 10;

// The maximum number of characters to show in the feed item title.
const maxTitleCount = 1024;

// Find the token and target origin for this conversation from the HTML. The
// token is used for secure communication, and is generated and stuffed into the
// frame by subscribe.js.
let token = '';
let targetOrigin = '';
const html = document.documentElement.outerHTML;
const startTag = '<!--Token:';
let tokenStart = html.indexOf(startTag);
if (tokenStart > -1) {
  tokenStart += startTag.length;
  targetOrigin = html.substring(tokenStart, tokenStart + 32);
  tokenStart += 32;
  const tokenEnd = html.indexOf('-->', tokenStart);
  if (tokenEnd > tokenStart) {
    token = html.substring(tokenStart, tokenEnd);
  }
}

if (token.length > 0) {
  const mc = new MessageChannel();
  window.parent.postMessage(
      token,
      'chrome-extension:/' +
          '/' + targetOrigin,
      [mc.port2]);
  mc.port1.onmessage = function(event) {
    const parser = new DOMParser();
    const doc = parser.parseFromString(event.data, 'text/xml');
    if (doc) {
      buildPreview(doc);
    } else {
      // Already handled in subscribe.html.
    }
  };
}

function buildPreview(doc) {
  // Start building the part we render inside an IFRAME. We use a table to
  // ensure that items are separated vertically from each other.
  const table = document.createElement('table');
  const tbody = document.createElement('tbody');
  table.appendChild(tbody);

  // Now parse the rest. Some use <entry> for each feed item, others use
  // <channel><item>.
  let entries = doc.getElementsByTagName('entry');
  if (entries.length === 0) {
    entries = doc.getElementsByTagName('item');
  }

  for (i = 0; i < entries.length && i < maxFeedItems; ++i) {
    item = entries.item(i);

    // Grab the title for the feed item.
    let itemTitle = item.getElementsByTagName('title')[0];
    if (itemTitle) {
      itemTitle = itemTitle.textContent;
    } else {
      itemTitle = 'Unknown title';
    }

    // Ensure max length for title.
    if (itemTitle.length > maxTitleCount) {
      itemTitle = itemTitle.substring(0, maxTitleCount) + '...';
    }

    // Grab the description.
    // TODO(aa): Do we need to check for type=html here?
    let itemDesc = item.getElementsByTagName('description')[0];
    if (!itemDesc) {
      itemDesc = item.getElementsByTagName('summary')[0];
    }
    if (!itemDesc) {
      itemDesc = item.getElementsByTagName('content')[0];
    }

    if (itemDesc) {
      itemDesc = itemDesc.textContent;
    } else {
      itemDesc = '';
    }

    // Grab the link URL.
    const itemLink = item.getElementsByTagName('link');
    let link = '';
    if (itemLink.length > 0) {
      link = itemLink[0].childNodes[0];
      if (link) {
        link = itemLink[0].childNodes[0].nodeValue;
      } else {
        link = itemLink[0].getAttribute('href');
      }
    }

    const tr = document.createElement('tr');
    const td = document.createElement('td');

    const sanitizer = window.domAutomationController === undefined ?
        new goog.html.sanitizer.HtmlSanitizer.Builder()
            .withCustomNetworkRequestUrlPolicy(goog.html.SafeUrl.sanitize)
            .withCustomUrlPolicy(goog.html.SafeUrl.sanitize)
            .build() :
        goog.html.sanitizer.HtmlSanitizer;

    // If we found a link we'll create an anchor element,
    // otherwise just use a bold headline for the title.
    const anchor = (link !== '') ? document.createElement('a') :
                                   document.createElement('strong');
    anchor.id = 'anchor_' + String(i);
    if (link !== '') {
      goog.dom.safe.setAnchorHref(anchor, goog.html.SafeUrl.sanitize(link));
    }
    goog.dom.safe.setInnerHtml(anchor, sanitizer.sanitize(itemTitle));
    anchor.target = '_top';
    anchor.className = 'item_title';

    const span = document.createElement('span');
    span.id = 'desc_' + String(i);
    span.className = 'item_desc';
    goog.dom.safe.setInnerHtml(span, sanitizer.sanitize(itemDesc));

    td.appendChild(anchor);
    td.appendChild(document.createElement('br'));
    td.appendChild(span);
    td.appendChild(document.createElement('br'));
    td.appendChild(document.createElement('br'));

    tr.appendChild(td);
    tbody.appendChild(tr);
  }

  table.appendChild(tbody);
  document.body.appendChild(table);

  if (window.domAutomationController) {
    window.domAutomationController.send('PreviewReady');
  }
}
