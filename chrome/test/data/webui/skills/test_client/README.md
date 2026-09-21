<!--
Copyright 2026 The Chromium Authors
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
-->

# Skills Test Client (V2)

This test client facilitates manual and automated testing of the API and postMessage bridge between the Chrome Skills WebUI host (`chrome://skills`) and the remote guest web application (`SkillsWebViewV2Enabled`).

## Quickstart

### Build and Run the Test Server

```bash
chrome/test/data/webui/skills/test_client/serve.py -C out/Default
```

Options:
- `-C`, `--outdir`: Path to the build output directory (required).
- `-p`, `--port`: Port to listen on (default: `8000`).
- `-n`, `--nobuild`: Skip the `autoninja` build step.
- `--bind-all-interfaces`: Listen on all network interfaces instead of `localhost` only.

### Launch Chrome with Test Client Flags

```bash
out/Default/chrome \
    --enable-features=SkillsWebViewV2Enabled,SkillsEnabled \
    --skills-v2-origin=http://localhost:8000
```

### Open Skills

1. Open `chrome://skills` (or `chrome://skills/dialog`) in Chrome.
2. The Skills WebUI will embed the test client inside a `<webview>` tag, connect via postMessage handshake, and display the test client controls.
3. Use the test client buttons to trigger toasts, skill invocation, prompt forwarding, full page editor opening, performance metrics, and navigation simulation.

### Command Line Flags Explanation

| Flag | Purpose |
| --- | --- |
| `--enable-features=SkillsWebViewV2Enabled,SkillsEnabled` | Enables the Skills V2 Webview implementation (`SkillsWebViewV2Enabled`) and the base Skills feature (`SkillsEnabled`). |
| `--skills-v2-origin=http://localhost:8000` | Sets the primary remote origin for the Skills V2 Webview guest to point to the local test server. |

