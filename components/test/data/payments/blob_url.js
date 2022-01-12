/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** Requests payment via a blob URL. */
function buy() { // eslint-disable-line no-unused-vars
  var spoof = function() {
    var payload =
        'PGh0bWw+PGhlYWQ+PG1ldGEgbmFtZT0idmlld3BvcnQiIGNvbnRlbnQ9IndpZHRoPWRldmljZS13aWR0aCwgaW5pdGlhbC1zY2FsZT0yLCBtYXhpbXVtLXNjYWxlPTIiPjwvaGVhZD48Ym9keT48ZGl2IGlkPSJyZXN1bHQiPjwvZGl2PjxzY3JpcHQ+dHJ5IHsgIG5ldyBQYXltZW50UmVxdWVzdChbe3N1cHBvcnRlZE1ldGhvZHM6ICJiYXNpYy1jYXJkIn1dLCAgICB7dG90YWw6IHtsYWJlbDogIlQiLCBhbW91bnQ6IHtjdXJyZW5jeTogIlVTRCIsIHZhbHVlOiAiMS4wMCJ9fX0pICAuc2hvdygpICAudGhlbihmdW5jdGlvbihpbnN0cnVtZW50UmVzcG9uc2UpIHsgICAgZG9jdW1lbnQuZ2V0RWxlbWVudEJ5SWQoInJlc3VsdCIpLmlubmVySFRNTCA9ICJSZXNvbHZlZCI7ICB9KS5jYXRjaChmdW5jdGlvbihlKSB7ICAgIGRvY3VtZW50LmdldEVsZW1lbnRCeUlkKCJyZXN1bHQiKS5pbm5lckhUTUwgPSAiUmVqZWN0ZWQ6ICIgKyBlOyAgfSk7fSBjYXRjaChlKSB7ICBkb2N1bWVudC5nZXRFbGVtZW50QnlJZCgicmVzdWx0IikuaW5uZXJIVE1MID0gIkV4Y2VwdGlvbjogIiArIGU7fTwvc2NyaXB0PjwvYm9keT48L2h0bWw+'; // eslint-disable-line max-len
    document.write(atob(payload));
  };
  window.location.href =
      URL.createObjectURL(new Blob(['<script>(', spoof, ')();</script>'], {
        type: 'text/html',
      }));
}
