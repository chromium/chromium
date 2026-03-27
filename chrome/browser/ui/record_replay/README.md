# Record Replay - Browser UI

This directory (`chrome/browser/ui/record_replay`) contains the browser-process
UI components for the **Record Replay** feature in Chromium.

While `chrome/browser/record_replay` handles the core coordination and state
management, this directory focuses on the user-facing interface elements, such
as the dialogs and bubbles used to name and save recordings.

For a complete understanding of the multi-process architecture and coordination
logic, please refer to the primary documentation node:
[chrome/browser/record_replay/README.md](../../browser/record_replay/README.md)

## Key Components

- **`SaveRecordingBubbleController`**: Manages the lifecycle and user
  interactions for the bubble shown when a recording is completed, allowing the
  user to name and persist the session.

## Upkeep / Guidelines for AI Agents

NOTE: For detailed information about each component, please refer to the
documentation in the respective header files (*.h) within this directory.

> [!IMPORTANT] This document functions as a **living document**. AI agents
> introducing any major sub-features, changing process or Mojo interfaces, or
> solving significant ambiguities **MUST** update this file to reflect the
> updated mental model of the feature tree.
