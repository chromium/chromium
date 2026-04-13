# DOM Activity Telemetry Tester

This project contains a Chrome extension and a test page designed to trigger specific DOM Access and Script Injection telemetry signals for testing the underlying Chromium C++ instrumentation.

## 1. Load the Extension
1. Open Google Chrome and navigate to `chrome://extensions/`.
2. Enable **Developer mode** in the top right corner.
3. Click **Load unpacked** and select the path to the test extension.
   `<path-to-chromium-src>/chrome/test/data/safe_browsing/extension_telemetry/dom_activity/extension`
4. **Important**: Once loaded, find the "DOM Activity Telemetry Tester" card, click **Details**, and toggle **Allow access to file URLs** to `ON`. This is required for the extension to interact with a local `index.html` file.

## 2. Load the Test Page
You can load the test page either by directly opening the local file, or by hosting it on a local Python server.

**Option A: Local File Path (Requires "Allow access to file URLs")**
1. Paste the following path into your Chrome URL bar:
   `file:///<path-to-chromium-src>/chrome/test/data/safe_browsing/extension_telemetry/dom_activity/test-page/index.html`

**Option B: Python HTTP Server (Recommended)**
If you do not want to enable "Allow access to file URLs", you can host the page locally.
1. Open a terminal and run the following commands:
   ```bash
   cd <path-to-chromium-src>/chrome/test/data/safe_browsing/extension_telemetry/dom_activity/test-page
   python3 -m http.server 8000
   ```
2. In Chrome, navigate to `http://localhost:8000`.

Keep this test page tab active for the next step.

## 3. Generate Events
1. With the test page active, click the **DOM Activity Telemetry Tester** extension icon in your Chrome toolbar.
2. The popup menu categorizes the triggers:
   - **Confidentiality (DOM Access)**: Buttons here will read cookies and input values, triggering `kDOMAccess` signals.
   - **Integrity (Script Injection)**: Buttons here will inject scripts via `chrome.scripting.executeScript`, triggering `kScriptInjection` signals.
   - **Integrity (DOM Injection)**: Buttons here will maliciously manipulate the test page's DOM (e.g., adding `<script src>`, `<iframe>`, or changing object `action`/`href` attributes), triggering `kScriptInjection` signals.
3. Click any button to execute the manipulation. You can open the Chrome DevTools (`F12`) on the test page to see corresponding JavaScript console logs verifying that the DOM changes occurred.
