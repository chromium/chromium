// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const windowQueryOptions = { populate: true, windowTypes: ["normal"] };

function setApiReturnValue(text) {
    const apiReturnValueTextArea = document.getElementById(
        "api_return_value_text_area");
    apiReturnValueTextArea.value = text;

    // Log API return value to console as not all test results can be seen in
    // the text area, such as when the window is minimized.
    console.log(`API return value: ${text}`);
}

function getWindowIdFromTextInput(domElementId) {
    const domElement = document.getElementById(domElementId);
    if (domElement === null) {
        throw `DOM element ID doesn't exist: ${domElementId}`;
    }

    const windowIdString = domElement.value;
    if (!windowIdString) {
        throw `Please enter a window ID.`;
    }

    const windowId = parseInt(windowIdString);
    if (isNaN(windowId)) {
        throw `Invalid window ID: ${windowIdString}`;
    }

    return windowId;
}

function getWindowUpdateInfo() {
    const option =
        document.getElementById("update_window_options_select").value;

    switch (option) {
        case "focused_true":
            return { focused: true };
        case "focused_false":
            return { focused: false };
        case "resize_100_200_400_300":
            return { left: 100, top: 200, width: 400, height: 300 };
        case "state_normal":
            return { state: "normal" };
        case "state_minimized":
            return { state: "minimized" };
        case "state_maximized":
            return { state: "maximized" };
        case "state_fullscreen":
            return { state: "fullscreen" };
        case "state_locked_fullscreen":
            return { state: "locked-fullscreen" };
        case "drawAttention_true":
            return { drawAttention: true };
        case "drawAttention_false":
            return { drawAttention: false };
        default:
            throw `Unsupported option: ${option}`;
    }
}

// Populate current window ID into each text input box for an window ID.
async function initWindowIdTextInput() {
    try {
        const window = await chrome.windows.getCurrent();
        document.getElementById("get_window_id_input").value = window.id;
        document.getElementById("update_window_id_input").value = window.id;
    } catch (error) {
        setApiReturnValue(
            `Unable to initialize window ID text input: ${error}`);
    }
}

await initWindowIdTextInput();

document.getElementById("get_all_windows_button").onclick = async () => {
    const windows = await chrome.windows.getAll(windowQueryOptions);
    const result =
        `All windows: ${windows.length}\n\n${JSON.stringify(windows, null, 2)}`;
    setApiReturnValue(result);
};

document.getElementById("get_current_window_button").onclick = async () => {
    const window = await chrome.windows.getCurrent(windowQueryOptions);
    const result = `Current window:\n\n${JSON.stringify(window, null, 2)}`;
    setApiReturnValue(result);
};

document.getElementById("get_last_focused_window_button").onclick =
    async () => {
        const window = await chrome.windows.getLastFocused(windowQueryOptions);
        const result =
            `Last focused window:\n\n${JSON.stringify(window, null, 2)}`;
        setApiReturnValue(result);
    };

document.getElementById("get_window_button").onclick = async () => {
    try {
        const windowId = getWindowIdFromTextInput("get_window_id_input");
        const window = await chrome.windows.get(windowId, windowQueryOptions);
        const result =
            `Window ${windowId}: ${JSON.stringify(window, null, 2)}`;
        setApiReturnValue(result);
    } catch (error) {
        setApiReturnValue(`${error}`);
    }
};

document.getElementById("update_window_button").onclick = async () => {
    try {
        const windowId = getWindowIdFromTextInput("update_window_id_input");
        const updateInfo = getWindowUpdateInfo();
        const window = await chrome.windows.update(windowId, updateInfo);
        const result =
            `Updated window ${windowId}: ${JSON.stringify(window, null, 2)}`;
        setApiReturnValue(result);
    } catch (error) {
        setApiReturnValue(`${error}`);
    }
};
