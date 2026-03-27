# Record Replay - Renderer Agent

This directory (`chrome/renderer/record_replay`) contains the renderer-process
agent code for the **Record Replay** feature in Chromium.

While `chrome/browser/record_replay` handles the central coordination, state
machine, and persistence, this directory provides the necessary implementation
to perform actions in the renderer. It observes user interactions (clicks, text
input, selections) on the web page and reports them back via Mojo to the browser
process, which creates the actual recording. It also executes commands sent from
the browser process to replay actions within the DOM.

For a complete understanding of the multi-process architecture and coordination
logic, please refer to the primary documentation node:
[chrome/browser/record_replay/README.md](../../browser/record_replay/README.md)

## Upkeep / Guidelines for AI Agents

NOTE: For detailed information about each component, please refer to the
documentation in the respective header files (\*.h) within this directory.

> [!IMPORTANT] This document functions as a **living document**. AI agents
> introducing any major sub-features, changing process or Mojo interfaces, or
> solving significant ambiguities **MUST** update this file to reflect the
> updated mental model of the feature tree.
