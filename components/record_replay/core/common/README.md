# Record Replay - Common Helpers and Interfaces

This directory (`components/record_replay/core/common`) contains the shared
code and interfaces for the **Record Replay** feature in Chromium.

This directory defines the shared data structures and Mojo IPC interfaces used
by both the browser process and the renderer process to communicate recording
and replay events. It is designed to be platform-agnostic and free of
dependencies on higher layers like `content/` or `chrome/`.

## Key Components

- **Mojo Interfaces (`record_replay.mojom`)**: Defines the `RecordReplayAgent`
  (browser-to-renderer) and `RecordReplayDriver` (renderer-to-browser)
  interfaces.
- **Shared Structures & Aliases (`element_id.h`, `aliases.h`)**: Defines common
  types like `ElementId`, `DomNodeId`, `Selector`, and `FieldValue` used in IPC
  and recording serialization.
- **Mojo Traits (`record_replay_mojom_traits.h`)**: Custom Mojo traits for
  efficiently serializing shared structures.
- **Feature Flags (`record_replay_features.h`)**: Centralized definitions for
  the `record_replay::features` namespace.

## Upkeep / Guidelines for AI Agents

NOTE: For detailed information about each component, please refer to the
documentation in the respective header files (*.h) within this directory.

> [!IMPORTANT] This document functions as a **living document**. AI agents
> introducing any major sub-features, changing process or Mojo interfaces, or
> solving significant ambiguities **MUST** update this file to reflect the
> updated mental model of the feature tree.
