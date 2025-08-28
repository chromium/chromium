// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setApiReturnValue(text) {
    const apiReturnValueTextArea = document.getElementById(
        "api_return_value_text_area");
    apiReturnValueTextArea.value = text;
}

document.getElementById("get_all_windows_button").onclick = async () => {
    const windows = await chrome.windows.getAll();
    const result =
        `All windows: ${windows.length}\n\n${JSON.stringify(windows, null, 2)}`;
    setApiReturnValue(result);
};

document.getElementById("get_current_window_button").onclick = async () => {
    const window = await chrome.windows.getCurrent();
    const result = `Current window:\n\n${JSON.stringify(window, null, 2)}`;
    setApiReturnValue(result);
};

document.getElementById("get_last_focused_window_button").onclick =
    async () => {
        const window = await chrome.windows.getLastFocused();
        const result =
            `Last focused window:\n\n${JSON.stringify(window, null, 2)}`;
        setApiReturnValue(result);
    }

document.getElementById("get_window_button").onclick = async () => {
    const windowIdString = document.getElementById("window_id_input").value;
    if (!windowIdString) {
        setApiReturnValue("Please enter a window ID.");
        return;
    }

    const windowId = parseInt(windowIdString);
    if (isNaN(windowId)) {
        setApiReturnValue(`Invalid window ID: ${windowIdString}`);
        return;
    }

    try {
        const window = await chrome.windows.get(windowId);
        const result =
            `Window with ID ${windowId}: ${JSON.stringify(window, null, 2)}`;
        setApiReturnValue(result);
    } catch (error) {
        setApiReturnValue(`${error}`);
    }
}
