# Record Replay - Core Browser Component

This directory (`components/record_replay/core/browser`) contains the
platform-agnostic browser-process coordinator code for the **Record Replay**
feature in Chromium.

This system is used for observing, recording, and serializing user actions
(clicks, selects, text updates, Autofill interactions) on a web page, and
subsequently replaying them.

This subcomponent is designed to be independent of specific UI implementations
(like Chrome's views) or content-specific identifiers (like `RenderFrameHost`).
It interfaces with these layers through the `RecordReplayClient` and
`RecordReplayDriver` abstractions.

## Key Components

- **`RecordReplayManager`**: The central coordinator that holds the state
  machine (`kIdle`, `kRecording`, `kReplaying`). It manages the active
  `Recorder` or `Replayer` instance.
- **`Recorder`**: Manages the assembly and serialization of a recording session
  into a `Recording` proto.
- **`Replayer`**: Executes a previously recorded session by commanding the
  renderer to perform specific actions at the recorded times.
- **`RecordingDataManager`**: An interface for persistent storage of recorded
  sessions.
- **`TaskDatabase`**: A relational SQLite database that stores metadata
  about recordings, standalone `TaskDefinition` intent analysis, and
  sensitive `TaskData` values. It supports efficient lookup by site and
  handles database migrations and seeding from Finch parameters.
- **Abstractions (`record_replay_client.h`, `record_replay_driver.h`, `record_replay_driver_factory.h`)**:
  Abstract interfaces that allow the core logic to communicate with the content
  and chrome layers without being directly dependent on them.

## Object Ownership & Lifecycle

- **Persistent Storage**: `RecordingDataManager` is typically tied to a user
  profile.
- **State Management**: `RecordReplayManager` is typically tied to a tab.
- **Active Sessions**: `Recorder` and `Replayer` are transient objects created
  and destroyed by the manager for a single session.

## Threading & Sequencing

All coordinator logic in this directory runs on the **UI thread**. Persistence
operations in the database are handled asynchronously on a dedicated background
sequence.

## Upkeep / Guidelines for AI Agents

NOTE: For detailed information about each component, please refer to the
documentation in the respective header files (*.h) within this directory.

> [!IMPORTANT] This document functions as a **living document**. AI agents
> introducing any major sub-features, changing process or Mojo interfaces, or
> solving significant ambiguities **MUST** update this file to reflect the
> updated mental model of the feature tree.
