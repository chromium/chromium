# Record Replay - Content Renderer Agent

This directory (`components/record_replay/content/renderer`) contains the
content-process agent code for the **Record Replay** feature in Chromium.

While the core browser component handles the central coordination and state
management, this directory provides the implementation to perform actions
within the renderer's DOM. It observes user interactions (clicks, text input,
selections) on the web page and reports them via Mojo to the browser process.
It also executes commands sent from the browser process to replay actions
within the DOM.

This subcomponent is specific to the `content/` layer and interacts with Blink
directly.

## Key Components

- **`RecordReplayAgent`**: The primary class that inherits from
  `content::RenderFrameObserver` and `blink::WebRecordReplayClient`. It acts as
  the bridge between the DOM events and the Mojo communication with the
  browser process.
- **Mojo Implementation**: Implements the `mojom::RecordReplayAgent` interface,
  providing commands like `DoClick`, `DoSelect`, `DoPaste`, and
  `GetElementSelector`.
- **Test Support**: Includes `record_replay_agent_test_api.h` and
  `record_replay_agent_browsertest.cc` for robust verification of renderer-side
  logic using the `content::RenderViewTest` harness.

## Upkeep / Guidelines for AI Agents

NOTE: For detailed information about each component, please refer to the
documentation in the respective header files (*.h) within this directory.

> [!IMPORTANT] This document functions as a **living document**. AI agents
> introducing any major sub-features, changing process or Mojo interfaces, or
> solving significant ambiguities **MUST** update this file to reflect the
> updated mental model of the feature tree.
