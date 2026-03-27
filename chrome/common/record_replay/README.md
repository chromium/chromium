# Record Replay - Common Helpers and Interfaces

This directory (`chrome/common/record_replay`) contains the shared code and
interfaces for the **Record Replay** feature in Chromium.

This directory defines the shared data structures and Mojo IPC interfaces used
by both the browser process (`chrome/browser/record_replay`) and the renderer
process (`chrome/renderer/record_replay`) to communicate recording and replay
events.

For a complete understanding of the multi-process architecture and coordination
logic, please refer to the primary documentation node:
[chrome/browser/record_replay/README.md](../../browser/record_replay/README.md)

## Key Components

- **Mojo Interfaces (`record_replay.mojom`)**: Defines the `RecordReplayAgent`
  (browser-to-renderer) and `RecordReplayDriver` (renderer-to-browser)
  interfaces.
- **Shared Structures**: Defines common types like `DomNodeId`, `Selector`, and
  `FieldValue` used in IPC and recording serialization.
- **Feature Flags (`record_replay_features.h`)**: Centralized definitions for
  the `record_replay::features` namespace.

## Upkeep / Guidelines for AI Agents

NOTE: For detailed information about each component, please refer to the
documentation in the respective header files (*.h) within this directory.

> [!IMPORTANT] This document functions as a **living document**. AI agents
> introducing any major sub-features, changing process or Mojo interfaces, or
> solving significant ambiguities **MUST** update this file to reflect the
> updated mental model of the feature tree.
