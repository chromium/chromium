// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const windowQueryOptions = { populate: true, windowTypes: ["normal", "popup"] };

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
    const option = document.querySelector(
        'input[name="update_window_option"]:checked').value;

    switch (option) {
        case "focus":
            const focusValue = document.querySelector('input[name="focus_value"]:checked').value === "true";
            return { focused: focusValue };
        case "resize":
            const left = parseInt(document.getElementById("resize_left").value);
            const top = parseInt(document.getElementById("resize_top").value);
            const width = parseInt(document.getElementById("resize_width").value);
            const height = parseInt(document.getElementById("resize_height").value);
            return { left, top, width, height };
        case "state":
            const stateValue = document.querySelector('input[name="state_value"]:checked').value;
            return { state: stateValue };
        case "drawAttention":
            const drawAttentionValue = document.querySelector('input[name="drawAttention_value"]:checked').value === "true";
            return { drawAttention: drawAttentionValue };
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
        document.getElementById("remove_window_id_input").value = window.id;
    } catch (error) {
        setApiReturnValue(
            `Unable to initialize window ID text input: ${error}`);
    }
}

// For a window update option, only show the UI elements for its parameter
// values when that update option is selected.
//
// For example, the "focus" option has parameter value "true" or "false".
// We only show the radio buttons to select "true" or "false" when "focus"
// is selected.
function updateVisibilityForWindowUpdateOptions() {
    const options = {
        'focus': 'focus_options',
        'resize': 'resize_options',
        'state': 'state_options',
        'drawAttention': 'drawAttention_options',
    };

    const checkedValue = document.querySelector('input[name="update_window_option"]:checked').value;

    for (const option in options) {
        document.getElementById(options[option]).style.display =
            (option === checkedValue) ? 'block' : 'none';
    }
}

await initWindowIdTextInput();
updateVisibilityForWindowUpdateOptions();

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

document.getElementById("remove_window_button").onclick = async () => {
    try {
        const windowId = getWindowIdFromTextInput("remove_window_id_input");
        await chrome.windows.remove(windowId);
    } catch (error) {
        setApiReturnValue(`${error}`);
    }
}

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

document.getElementById('update_window_options_radio_group').addEventListener('change', updateVisibilityForWindowUpdateOptions);
