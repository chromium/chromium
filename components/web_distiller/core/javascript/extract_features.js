// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
function hasOGArticle() {
  const elems = document.head.querySelectorAll(
      'meta[property="og:type"],meta[name="og:type"]');
  for (const i in elems) {
    if (elems[i].content && elems[i].content.toUpperCase() === 'ARTICLE') {
      return true;
    }
  }
  return false;
}

const body = document.body;
if (!body) {
  return false;
}
return JSON.stringify({
  'opengraph': hasOGArticle(),
  'url': document.location.href,
  'numElements': body.querySelectorAll('*').length,
  'numAnchors': body.querySelectorAll('a').length,
  'numForms': body.querySelectorAll('form').length,
  'innerText': body.innerText,
  'textContent': body.textContent,
  'innerHTML': body.innerHTML,
});
})();
