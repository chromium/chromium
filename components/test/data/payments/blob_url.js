/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** Requests payment via a blob URL. */
function buy() {
  const spoof = function() {
    // base64-encoded HTML page that defines a function, triggerPaymentRequest,
    // which creates a basic-card PaymentRequest and calls show() on it.
    const payload =
        'PGh0bWw+Cgo8aGVhZD4KICAgIDxtZXRhIG5hbWU9InZpZXdwb3J0IiBjb250ZW50PSJ3aWR0aD1kZXZpY2Utd2lkdGgsIGluaXRpYWwtc2NhbGU9MiwgbWF4aW11bS1zY2FsZT0yIj4KPC9oZWFkPgoKPGJvZHk+CiAgICA8ZGl2IGlkPSJyZXN1bHQiPjwvZGl2PgogICAgPHNjcmlwdD5hc3luYyBmdW5jdGlvbiB0cmlnZ2VyUGF5bWVudFJlcXVlc3QoKSB7IHRyeSB7IGF3YWl0IG5ldyBQYXltZW50UmVxdWVzdChbeyBzdXBwb3J0ZWRNZXRob2RzOiAiaHR0cHM6Ly9nb29nbGUuY29tL3BheSIgfV0sIHsgdG90YWw6IHsgbGFiZWw6ICJUIiwgYW1vdW50OiB7IGN1cnJlbmN5OiAiVVNEIiwgdmFsdWU6ICIxLjAwIiB9IH0gfSkuc2hvdygpLnRoZW4oZnVuY3Rpb24gKGluc3RydW1lbnRSZXNwb25zZSkgeyBkb2N1bWVudC5nZXRFbGVtZW50QnlJZCgicmVzdWx0IikuaW5uZXJIVE1MID0gIlJlc29sdmVkIjsgfSk7IH0gY2F0Y2ggKGUpIHsgZG9jdW1lbnQuZ2V0RWxlbWVudEJ5SWQoInJlc3VsdCIpLmlubmVySFRNTCA9ICJFeGNlcHRpb246ICIgKyBlOyByZXR1cm4gZS50b1N0cmluZygpOyB9IH08L3NjcmlwdD4KPC9ib2R5PgoKPC9odG1sPg==';  // eslint-disable-line max-len
    document.write(atob(payload));
  };
  window.location.href =
      URL.createObjectURL(new Blob(['<script>(', spoof, ')();</script>'], {
        type: 'text/html',
      }));
}
