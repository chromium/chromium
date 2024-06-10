// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var dataUrl = `data:text/html,
<body>
  <p>Input:</p>
  <input type="text" id="text_id">
  <script>
    var text_input = document.getElementById('text_id');
    text_input.focus();

    var textInputPromise = new Promise((resolve) => {
      text_input.addEventListener('input', resolve);
    });

    async function waitForInput() {
      await textInputPromise;
    }
  </script>
</body>
`;

var webview = document.createElement('webview');
webview.style.border = 'solid';
webview.src = dataUrl;
webview.addEventListener('loadstop', () => {
  webview.focus();
});
document.body.appendChild(webview);
