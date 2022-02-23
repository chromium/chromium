/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** Requests payment via a blob URL. */
function buy() { // eslint-disable-line no-unused-vars
  var spoof = function() {
    // base64-encoded HTML page that defines a function, triggerPaymentRequest,
    // which creates a basic-card PaymentRequest and calls show() on it.
    var payload =
        'PGh0bWw+PGhlYWQ+PG1ldGEgbmFtZT0idmlld3BvcnQiIGNvbnRlbnQ9IndpZHRoPWRldmljZS13aWR0aCwgaW5pdGlhbC1zY2FsZT0yLCBtYXhpbXVtLXNjYWxlPTIiPjwvaGVhZD48Ym9keT48ZGl2IGlkPSJyZXN1bHQiPjwvZGl2PjxzY3JpcHQ+ZnVuY3Rpb24gdHJpZ2dlclBheW1lbnRSZXF1ZXN0KCkgeyB0cnkgeyAgbmV3IFBheW1lbnRSZXF1ZXN0KFt7c3VwcG9ydGVkTWV0aG9kczogImJhc2ljLWNhcmQifV0sICAgIHt0b3RhbDoge2xhYmVsOiAiVCIsIGFtb3VudDoge2N1cnJlbmN5OiAiVVNEIiwgdmFsdWU6ICIxLjAwIn19fSkgIC5zaG93KCkgIC50aGVuKGZ1bmN0aW9uKGluc3RydW1lbnRSZXNwb25zZSkgeyAgICBkb2N1bWVudC5nZXRFbGVtZW50QnlJZCgicmVzdWx0IikuaW5uZXJIVE1MID0gIlJlc29sdmVkIjsgIH0pLmNhdGNoKGZ1bmN0aW9uKGUpIHsgICAgZG9jdW1lbnQuZ2V0RWxlbWVudEJ5SWQoInJlc3VsdCIpLmlubmVySFRNTCA9ICJSZWplY3RlZDogIiArIGU7ICB9KTt9IGNhdGNoKGUpIHsgIGRvY3VtZW50LmdldEVsZW1lbnRCeUlkKCJyZXN1bHQiKS5pbm5lckhUTUwgPSAiRXhjZXB0aW9uOiAiICsgZTt9fTwvc2NyaXB0PjwvYm9keT48L2h0bWw+'; // eslint-disable-line max-len
    document.write(atob(payload));
  };
  window.location.href =
      URL.createObjectURL(new Blob(['<script>(', spoof, ')();</script>'], {
        type: 'text/html',
      }));
}
