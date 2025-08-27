// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.getElementById("get_all_windows_button").onclick = async () => {
    const windows = await chrome.windows.getAll();

    const numWindows = document.getElementById("num_windows");
    numWindows.textContent = windows.length;
};
